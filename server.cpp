#include "server.hpp"
#include "Channel.hpp"
#include "Client.hpp"

Server::Server(int port, std::string password)
{
    this->srv_port = port;
    this->srv_password = password;
}

Server::~Server(){}

void Server::init_server()
{
    pollfd pfd;
    this->srv_socket = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(this->srv_socket, F_SETFL, O_NONBLOCK);

    if (this->srv_socket == -1)
        throw std::runtime_error("Error: could not create socket, retrying...");

    int reuseaddr = 1;
    if (setsockopt(this->srv_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1)
        throw std::runtime_error("Error: could not set socket options, retrying...");

    struct sockaddr_in srv_addr;
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(this->srv_port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(this->srv_socket, reinterpret_cast<struct sockaddr*>(&srv_addr), sizeof(srv_addr)) == -1)
        throw std::runtime_error("Error: could not bind socket, retrying...");

    if (listen(this->srv_socket, SOMAXCONN) == -1)
        throw std::runtime_error("Error: could not listen on socket, retrying...");
    
    pfd.fd = this->srv_socket;
    pfd.events = POLLIN;
    pfd.revents = 0;
    this->pollfds.push_back(pfd);
}

void Server::accept_client()
{
    pollfd pfd;

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    int client_socket = accept(this->srv_socket, (struct sockaddr*)&client_addr, &client_addr_size);
    if(client_socket == -1)
    {
        std::cerr << "Error: could not accept client." << std::endl; return;
    }
    std::cout << "Client connected." << std::endl;
    Client client(client_socket, client_addr);
    this->clients.insert(std::pair<int, Client>(client_socket, client));

    pfd.fd = client_socket;
    pfd.events = POLLIN;
    pfd.revents = 0;

    this->pollfds.push_back(pfd);
}

std::string Server::client_request(int client_socket)
{
    char buffer[1024];
    int bytes_read = recv(client_socket, buffer, 1024, 0);
    buffer[bytes_read] = '\0';
    if (bytes_read == -1)
    {
        std::cerr << "Error: could not read from client." << std::endl;
        close(client_socket);
        return std::string();
    }
    else if (bytes_read == 0)
    {
        std::cout << "Client disconnected." << std::endl;
        close(client_socket);
        return std::string();
    }
    return std::string(buffer);
}


void Server::join_cmd(int client_socket, std::string buffer)
{
    if(this->clients[client_socket].get_pass_state() == NOPASS)
    {
        send(client_socket, "ERR PASS\r\n", 10, 0);
        return;
    }
    if(buffer.empty())
    {
        send(client_socket, "ERR JOIN\r\n", 10, 0);
        return;
    }
    std::string channel_name = buffer.substr(0, buffer.find(" "));
    buffer.erase(0, channel_name.length());
    if(buffer.empty())
    {
        send(client_socket, "ERR JOIN\r\n", 10, 0);
        return;
    }
    std::string channel_password = buffer.substr(0, buffer.find(" "));
    buffer.erase(0, channel_password.length());
    if(buffer.empty())
    {
        send(client_socket, "ERR JOIN\r\n", 10, 0);
        return;
    }

    for(std::vector<Channel>::iterator it = this->channels.begin(); it != this->channels.end(); ++it)
    {
        if(it->get_name() == channel_name)
        {
            if(it->get_password() == channel_password)
            {
                it->add_user(this->clients[client_socket]);
                send(client_socket, "JOIN OK\r\n", 9, 0);
                return;
            }
        }
        else
        {
            send(client_socket, "wrong pass\n" ,11, 0);
            return ;
        }
    }
    Channel channel(channel_name, channel_password);
    channel.set_admin(this->clients[client_socket]);
    this->channels.push_back(channel);
    send(client_socket, "JOIN OKK\r\n", 9, 0);
}

void Server::pass_cmd(int client_socket, std::string buffer)
{
    std::cout << buffer << std::endl;
    if(buffer == this->srv_password + '\n')//TODO remove the \n;
    {
        this->clients[client_socket].set_pass_state();
        send(client_socket, "PASS OK\r\n", 9, 0);
    }
    else
    {
        send(client_socket, "PASS ERR\r\n", 10, 0);
    }
}

void Server::nick_cmd(int client_socket, std::string buffer)
{
    if(this->clients[client_socket].get_pass_state() == NOPASS)
    {
        send(client_socket, "ERR PASS\r\n", 10, 0);
        return;
    }
    if(buffer.empty())
    {
        send(client_socket, "ERR NICK\r\n", 10, 0);
        return;
    }
    this->clients[client_socket].set_nickname(buffer);
    send(client_socket, "NICK OK\r\n", 9, 0);
}

void Server::user_cmd(int client_socket, std::string buffer)
{
    if(this->clients[client_socket].get_pass_state() == NOPASS)
    {
        send(client_socket, "ERR PASS\r\n", 10, 0);
        return;
    }
    if(buffer.empty())
    {
        send(client_socket, "ERR USER\r\n", 10, 0);
        return;
    }
    this->clients[client_socket].set_username(buffer);
    send(client_socket, "USER OK\r\n", 9, 0);
}

void Server::msg(int client_socket, std::string buffer)
{
    if(this->clients[client_socket].get_pass_state() == NOPASS)
    {
        send(client_socket, "ERR PASS\r\n", 10, 0);
        return;
    }

    std::string channel_name = buffer.substr(0, buffer.find(" "));
    buffer.erase(0, channel_name.length() + 1);

    std::string message = buffer.substr(0, buffer.find(" "));
    buffer.erase(0, message.length());

    for(std::vector<Channel>::iterator it = this->channels.begin(); it != this->channels.end(); ++it)
    {
        if(it->get_name() == channel_name)
        {
            it->send_message(message, client_socket);
            send(client_socket, "MSG OK\r\n", 8, 0);
            return;
        }
    }
    send(client_socket, "ERR MSG\r\n", 9, 0);
    return;
}

void Server::kick_cmd(int client_socket, std::string buffer)
{
    std::string ch = buffer.substr(0,buffer.find(" "));
    buffer.erase(0,ch.length()+1);
    std::string user = buffer.substr(0,buffer.find(" "));
    buffer.erase(0,user.length()+1);
    std::string reason = buffer.substr(0);
    for(std::vector<Channel>::iterator it = this->channels.begin(); it != this->channels.end(); ++it)
    {
        if(it->get_name() == ch)
        {
            if(it->get_admin().get_nickname() == this->clients[client_socket].get_nickname())
            {
                it->kick_user(user);
                send(client_socket, "KICK OK\r\n", 9, 0);
                return;
            }
            else
            {
                send(client_socket, "ERR KICK\r\n", 10, 0);
                return;
            }
        }
    }
}

void Channel::invite_user(std::string user)
{
    for(std::vector<Client>::iterator it = this->users.begin(); it != this->users.end(); ++it)
    {
        if(it->get_nickname() == user)
        {
            return;
        }
    }
}

void Server::invite_cmd(int client_socket, std::string buffer){
    std::string user = buffer.substr(0,buffer.find(" "));
    buffer.erase(0,user.length()+1);
    std::string ch = buffer.substr(0,buffer.find(" "));

    for(std::vector<Channel>::iterator it = this->channels.begin(); it != this->channels.end(); ++it)
    {
        if(it->get_name() == ch)
        {
            if(it->get_admin().get_nickname() == this->clients[client_socket].get_nickname())
            {
                it->invite_user(user);
                send(client_socket, "INVITE OK\r\n", 11, 0);
                return;
            }
            else
            {
                send(client_socket, "ERR INVITE\r\n", 12, 0);
                return;
            }
        }
    }

}

void Server::handle_input(int client_socket)
{   
    std::string buffer = this->client_request(client_socket);
    if(buffer.empty())
    {
        return;
    }
    // std::cout << buffer << std::endl;
    std::string command = buffer.substr(0, buffer.find(" "));
    buffer.erase(0, command.length() + 1);
    if(command == "PASS")
    {
        this->pass_cmd(client_socket, buffer);
    }
    else if(command == "INVITE")
    {
        this->invite_cmd(client_socket, buffer);
    }
    else if(command == "NICK")
    {
        this->nick_cmd(client_socket, buffer);
    }
    else if(command == "USER")
    {
        this->user_cmd(client_socket, buffer);
    }
    else if (command == "KICK")
    {
        this->kick_cmd(client_socket, buffer);
    }
    else if(command == "JOIN")
    {
        this->join_cmd(client_socket, buffer);
    }
    else if(command == "MSG")
    {
        this->msg(client_socket, buffer);
    }
    else
    {
        send(client_socket, "ERR CMD\r\n", 9, 0);
    }
}

void Server::poll_handler()
{
    for (size_t i = 0; i < this->pollfds.size(); i++)
    {
        if (this->pollfds[i].revents & POLLIN)
        {
            if (this->pollfds[i].fd == this->srv_socket)
            {
                this->accept_client();
            }
            else
            {
                handle_input(this->pollfds[i].fd);
            }
        }
    }
}

void Server::run()
{
    int poll_flag = 0;
    this->init_server();
    std::cout << "Server is listening on port " << this->srv_port << std::endl;

    while (true)
    {
        poll_flag = poll(this->pollfds.data(), this->pollfds.size(), 0);
        if(poll_flag == -1)
        {
            std::cerr << "Error: could not poll." << std::endl;
            continue;
        }
        if(poll_flag > 0)
        {
            this->poll_handler();
        }
    }
}

//TODO modify the messages sent to the users when executing authentification commands;
//TODO find a way to mark a user as authentificated after entering all auth commands;
//TODO remove the testing msg command;
//TODO polish and finish the functions you started (talking to myself here (griffin) );
//TODO this one is for drari, find something to work on, anything i started, ill finish, (dont modify code only add your own stuff, to avoid conflict);
// GGWP GUYS IRC DONE IN A LITTLE BIT;