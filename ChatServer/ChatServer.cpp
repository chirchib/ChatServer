#include <iostream>
#include <uwebsockets/App.h>
#include <string>
#include <algorithm>
#include <regex>
#include <map>
using namespace std;

/*
* 10 = Петя
* 11 = Вася
*
* Клиент пишет сообщение другому пользователя
* MESSAGE_TO::11::Привет от Пети
*
* Сервер отправит получателю сообщение
* MESSAGE_FROM::10::Привет от Пети
*
* Клиент захочет представиться
* SET_NAME::Вася
*
* Клиент захочет написать ВСЕМ
* MESSAGE_ALL::ВСЕМ ПРИВЕТ
*/

const string MESSAGE_TO = "MESSAGE_TO::";
const string SET_NAME = "SET_NAME::";

bool isSetName(string message) {
    return message.find(SET_NAME) == 0;
}

bool isCheckName(string message) {
    string rest = message.substr(SET_NAME.size());
    return rest.find("::") == -1 && rest.size() <= 255;
}

string parseName(string message) {
    return message.substr(SET_NAME.size());
}

// parseUserId("MESSAGE_TO::11::Привет от Пети") => 11
string parseUserId(string message) {
    string rest = message.substr(MESSAGE_TO.size()); // 11::Привет от Пети
    int pos = rest.find("::"); // pos = 2
    return rest.substr(0, pos); // "11"
}

// parseUserId("MESSAGE_TO::11::Привет от Пети") => "Привет от Пети"
string parseUserText(string message) {
    string rest = message.substr(MESSAGE_TO.size()); // 11::Привет от Пети
    int pos = rest.find("::"); // pos = 2
    return rest.substr(pos + 2); // "Привет от Пети"
}

bool isMessageTo(string message) {
    return message.find(MESSAGE_TO) == 0;
}

// messageFrom(11, "Petya", "Привет от пети") => MESSAGE_TO::11::[Petya] Привет от Пети
string messageFrom(string user_id, string senderName, string message) {
    return "MESSAGE_FROM::" + user_id + "::[" + senderName + "] " + message;
}

int main() {

    // Какую информацию о пользователе мы храним
    struct PerSocketData {
        string name; // имя
        unsigned int user_id; // уникальная идентификатор
    };

    unsigned int last_user_id = 10; // последний ид. пользователя
    unsigned int total_users = 0; // общее кол-во пользователей
    Bot bot; // объявляем бота

    // Настраиваем сервер
    uWS::App(). // Создаем простое приложение без шифрования
        ws<PerSocketData>("/*", { // Для каждого пользователя мы храним данные в виде PerSocketData
            /* Settings */
            .idleTimeout = 1200, // таймаут в секундах
            .open = [&last_user_id, &total_users](auto* ws) {
                // Функция open ("лямбда функция")
                // Вызывается при открытии соединения

                // 0. Получить структуру PerSocketData
                PerSocketData* userData = (PerSocketData*)ws->getUserData();
                // 1. Назначить пользователю уник. идент.
                userData->name = "UNNAMED";
                userData->user_id = last_user_id++;

                cout << "New user connected, id = " << userData->user_id << endl;
                cout << "Total users connected: " << ++total_users << endl;

                ws->subscribe("user#" + to_string(userData->user_id)); // у каждого юзера есть личный канал
                ws->subscribe("broadcast"); // подписываем юзера на общий канал
            },
            .message = [&last_user_id](auto* ws, string_view message, uWS::OpCode opCode) {

                // Вызывается при получении сообщения от пользователя
                string strMessage = string(message);
                // 0. Получить структуру PerSocketData
                PerSocketData* userData = (PerSocketData*)ws->getUserData();
                string authorId = to_string(userData->user_id);


                if (isMessageTo(strMessage)) {
                    // подготовить данные
                    string receiverId = parseUserId(strMessage);
                    string text = parseUserText(strMessage);
                    // userData->user_id == Отправитель

                    if (stoi(receiverId) <= last_user_id) {
                        string outgoingMessage = messageFrom(authorId, userData->name, text);
                        // отправить получателю
                        ws->publish("user#" + receiverId, outgoingMessage, uWS::OpCode::TEXT, false);

                        ws->send("Message sent", uWS::OpCode::TEXT);

                        cout << "Author #" << authorId << " wrote message to " << receiverId << endl;
                    }
                    else {
                        ws->send("Error, there is no user with ID = " + receiverId, uWS::OpCode::TEXT);
                        cout << "Author #" << authorId << " couldn't write message" << endl;
                    }
                }

                if (isSetName(strMessage)) {
                    if (isCheckName(strMessage)) {
                        string newName = parseName(strMessage);
                        userData->name = newName;
                        cout << "User #" << authorId << " set their name" << endl;
                    }
                    else cout << "User #" << authorId << " couldn't change their name" << endl;
                }
            },
            .close = [](auto*/*ws*/, int /*code*/, string_view /*message*/) {
                // Вызывается при отключении от сервера
            }
            })
        .listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    // Если все ок, вывести сообщение
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
            }).run(); // ЗАПУСК!
}

class Bot
{
private:
    string to_lower(string txt) {
        transform(txt.begin(), txt.end(), txt.begin(), ::tolower);
        return txt;
    }

    void botSay(string text) {
        cout << "[BOT]: " << text << endl;
    }

    string userAsk() {
        string question;
        cout << "[USER]: ";
        getline(cin, question);
        question = to_lower(question);
        return question;
    }

    map<string, string> database = {
        {"hello", "oh hi how are you?"},

        {"how are you", "Im doing just fine, LOL"},
        {"what are you doing", "I'm answering stupid question"},
        {"where are you from", "I'm from within your mind"},
        {"how old are you", "I'm only 10 nanoseconds old"},
        {"what are you", "I'm your friendly ChatBot2021"},

        {"exit", "Ok byyyyeeee"},
    };


    string question;

public:
    void setQuestion(string _question)
    {
        question = _question;
    }

    string getQuestion()
    {
        return question;
    }

    void botLogic()
    {
        while (question != "exit") {
            question = userAsk();
            int num_answers = 0;
            for (auto entry : database) {
                regex pattern = regex(".*" + entry.first + ".*");
                if (regex_match(question, pattern)) {
                    num_answers++;
                    botSay(entry.second);
                }
            }
            if (num_answers == 0) {
                botSay("Oiii, I don't knoooow");
            }
            if (num_answers > 5) {
                botSay("Oiii, I'm very talkative today");
            }
        }
    }
};