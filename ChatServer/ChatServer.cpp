#include <iostream>
#include <uwebsockets/App.h>
#include <map>
using namespace std;

/*
* 10 = ����
* 11 = ����
*
* ������ ����� ��������� ������� ������������
* MESSAGE_TO::11::������ �� ����
*
* ������ �������� ���������� ���������
* MESSAGE_FROM::10::������ �� ����
*
* ������ ������� �������������
* SET_NAME::����
*
* ������ ������� �������� ����
* MESSAGE_ALL::���� ������
* 
* ������ ����� �������� ������ ������������
* OFFLINE::22::Petya
* ONLINE::11::����
* 
*/


// ����� ���������� � ������������ �� ������
struct PerSocketData {
    string name; // ���
    unsigned int user_id; // ���������� �������������
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

// parseUserId("MESSAGE_TO::11::������ �� ����") => 11
string parseUserId(string message) {
    string rest = message.substr(MESSAGE_TO.size()); // 11::������ �� ����
    int pos = rest.find("::"); // pos = 2
    return rest.substr(0, pos); // "11"
}

// parseUserId("MESSAGE_TO::11::������ �� ����") => "������ �� ����"
string parseUserText(string message) {
    string rest = message.substr(MESSAGE_TO.size()); // 11::������ �� ����
    int pos = rest.find("::"); // pos = 2
    return rest.substr(pos + 2); // "������ �� ����"
}

bool isMessageTo(string message) {
    return message.find(MESSAGE_TO) == 0;
}

// messageFrom(11, "Petya", "������ �� ����") => MESSAGE_TO::11::[Petya] ������ �� ����
string messageFrom(string user_id, string senderName, string message) {
    return "MESSAGE_FROM::" + user_id + "::[" + senderName + "] " + message;
}

int main() {

    unsigned int last_user_id = 10; // ��������� ��. ������������

    // ����������� ������
    uWS::App(). // ������� ������� ���������� ��� ����������
        ws<PerSocketData>("/*", { // ��� ������� ������������ �� ������ ������ � ���� PerSocketData
            /* Settings */
            .idleTimeout = 1200, // ������� � ��������
            .open = [&last_user_id](auto* ws) {
                // ������� open ("������ �������")
                // ���������� ��� �������� ����������

                // 0. �������� ��������� PerSocketData
                PerSocketData* userData = (PerSocketData*)ws->getUserData();
                // 1. ��������� ������������ ����. �����.
                userData->name = "UNNAMED";
                userData->user_id = last_user_id++;
                
                updateName(userData);
                ws->publish(BROADCAST_CHANNEL, online(userData->user_id));

                cout << "[LOG] New user connected, id = " << userData->user_id << endl;
                cout << "[LOG] Total users connected: " << userNames.size() << endl;

                string user_channel = "user#" + to_string(userData->user_id);

                ws->subscribe("user#" + to_string(userData->user_id)); // � ������� ����� ���� ������ �����
                ws->subscribe(BROADCAST_CHANNEL); // ����������� ����� �� ����� �����
            

                for (auto entry : userNames) {
                    ws->send(online(entry.first), uWS::OpCode::TEXT);
                }
            },
            .message = [&last_user_id](auto* ws, string_view message, uWS::OpCode opCode) {

                // ���������� ��� ��������� ��������� �� ������������
                string strMessage = string(message);
                // 0. �������� ��������� PerSocketData
                PerSocketData* userData = (PerSocketData*)ws->getUserData();
                string authorId = to_string(userData->user_id);


                if (isMessageTo(strMessage)) {
                    // ����������� ������
                    string receiverId = parseUserId(strMessage);
                    string text = parseUserText(strMessage);
                    // userData->user_id == �����������

                    if (stoi(receiverId) <= last_user_id) {
                        string outgoingMessage = messageFrom(authorId, userData->name, text);
                        // ��������� ����������
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
                // ���������� ��� ���������� �� �������
                PerSocketData* userData = (PerSocketData*)ws->getUserData();

                ws->publish(BROADCAST_CHANNEL, offline(userData->user_id));
                deleteName(userData);

                cout << "[LOG] Total users connected: " << userNames.size() << endl;
            }
            })
        .listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    // ���� ��� ��, ������� ���������
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
            }).run(); // ������!
}   