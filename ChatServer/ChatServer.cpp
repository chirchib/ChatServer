#include <iostream>
#include <uwebsockets/App.h>
#include <string>
#include <algorithm>
#include <regex>
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

    // ����� ���������� � ������������ �� ������
    struct PerSocketData {
        string name; // ���
        unsigned int user_id; // ���������� �������������
    };

    unsigned int last_user_id = 10; // ��������� ��. ������������
    unsigned int total_users = 0; // ����� ���-�� �������������
    Bot bot; // ��������� ����

    // ����������� ������
    uWS::App(). // ������� ������� ���������� ��� ����������
        ws<PerSocketData>("/*", { // ��� ������� ������������ �� ������ ������ � ���� PerSocketData
            /* Settings */
            .idleTimeout = 1200, // ������� � ��������
            .open = [&last_user_id, &total_users](auto* ws) {
                // ������� open ("������ �������")
                // ���������� ��� �������� ����������

                // 0. �������� ��������� PerSocketData
                PerSocketData* userData = (PerSocketData*)ws->getUserData();
                // 1. ��������� ������������ ����. �����.
                userData->name = "UNNAMED";
                userData->user_id = last_user_id++;

                cout << "New user connected, id = " << userData->user_id << endl;
                cout << "Total users connected: " << ++total_users << endl;

                ws->subscribe("user#" + to_string(userData->user_id)); // � ������� ����� ���� ������ �����
                ws->subscribe("broadcast"); // ����������� ����� �� ����� �����
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
                // ���������� ��� ���������� �� �������
            }
            })
        .listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    // ���� ��� ��, ������� ���������
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
            }).run(); // ������!
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