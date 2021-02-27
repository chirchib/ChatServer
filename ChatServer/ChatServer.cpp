#include <iostream>
#include <uwebsockets/App.h>
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
* 
* Сервер будет сообщать статус пользователя
* OFFLINE::22::Petya
* ONLINE::11::Вася
* 
*/


// Какую информацию о пользователе мы храним
struct PerSocketData {
    string name; // имя
    unsigned int user_id; // уникальная идентификатор
};

map<unsigned int, string> userNames;

const string BROADCAST_CHANNEL = "broadcast";
const string MESSAGE_TO = "MESSAGE_TO::";
const string SET_NAME = "SET_NAME::";

const string OFFLINE = "OFFLINE::";
const string ONLINE = "ONLINE::";

// ONLINE::19::Vasya
string online(unsigned int user_id) {
    string name = userNames[user_id];
    return ONLINE + to_string(user_id) + "::" + name;
}

// OFFLINE::19::Vasya
string offline(unsigned int user_id) {
    string name = userNames[user_id];
    return OFFLINE + to_string(user_id) + "::" + name;
}

void updateName(PerSocketData* data) {
    userNames[data->user_id] = data->name;
}

void deleteName(PerSocketData* data) {
    userNames.erase(data->user_id);
}


bool isSetName(string message) {
    return message.find(SET_NAME) == 0;
}

bool isValidName(string message) {
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

    unsigned int last_user_id = 10; // последний ид. пользователя

    // Настраиваем сервер
    uWS::App(). // Создаем простое приложение без шифрования
        ws<PerSocketData>("/*", { // Для каждого пользователя мы храним данные в виде PerSocketData
            /* Settings */
            .idleTimeout = 1200, // таймаут в секундах
            .open = [&last_user_id](auto* ws) {
                // Функция open ("лямбда функция")
                // Вызывается при открытии соединения

                // 0. Получить структуру PerSocketData
                PerSocketData* userData = (PerSocketData*)ws->getUserData();
                // 1. Назначить пользователю уник. идент.
                userData->name = "UNNAMED";
                userData->user_id = last_user_id++;
                
                updateName(userData);
                ws->publish(BROADCAST_CHANNEL, online(userData->user_id));

                cout << "[LOG] New user connected, id = " << userData->user_id << endl;
                cout << "[LOG] Total users connected: " << userNames.size() << endl;

                string user_channel = "user#" + to_string(userData->user_id);

                ws->subscribe("user#" + to_string(userData->user_id)); // у каждого юзера есть личный канал
                ws->subscribe(BROADCAST_CHANNEL); // подписываем юзера на общий канал
            

                for (auto entry : userNames) {
                    ws->send(online(entry.first), uWS::OpCode::TEXT);
                }
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

                        cout << "[LOG] Author #" << authorId << " wrote message to " << receiverId << endl;
                    }
                    else {
                        ws->send("Error, there is no user with ID = " + receiverId, uWS::OpCode::TEXT);
                        cout << "[LOG] Author #" << authorId << " couldn't write message" << endl;
                    }
                }

                if (isSetName(strMessage)) {
                    if (isValidName(strMessage)) {
                        string newName = parseName(strMessage);
                        userData->name = newName;
                        
                        updateName(userData);
                        ws->publish(BROADCAST_CHANNEL, online(userData->user_id));

                        cout << "[LOG] User #" << authorId << " set their name" << endl;
                    }
                    else cout << "[LOG] User #" << authorId << " couldn't change their name" << endl;
                }
            },
            .close = [](auto* ws, int /*code*/, string_view /*message*/) {
                // Вызывается при отключении от сервера
                PerSocketData* userData = (PerSocketData*)ws->getUserData();

                ws->publish(BROADCAST_CHANNEL, offline(userData->user_id));
                deleteName(userData);

                cout << "[LOG] Total users connected: " << userNames.size() << endl;
            }
            })
        .listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    // Если все ок, вывести сообщение
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
            }).run(); // ЗАПУСК!
}   