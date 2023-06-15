#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include "Client.hpp"

class Client;
class Server;

class Channel{
    private:
        std::string name;
        std::string topic;
        std::string password;
        Client      admin;
        std::vector<Client> users;
        std::vector<Client> operators;
    public:
        Channel();
        Channel(std::string name, std::string topic);
        ~Channel();

        void set_name(std::string name);
        void set_topic(std::string topic);
        void set_admin(Client admin);
        void set_password(std::string password);

        std::string get_name() const;
        std::string get_topic() const;
        std::string get_password() const;
        Client get_admin() const;

        void add_user(Client user);
        void kick_user(std::string user);
        void invite_user(std::string user);
        std::string list_cmd(std::string msg);
        void add_operator(Client user);

        void remove_user(Client user);
        void remove_operator(Client user);

        void send_message(std::string message,int client_socket);
};


