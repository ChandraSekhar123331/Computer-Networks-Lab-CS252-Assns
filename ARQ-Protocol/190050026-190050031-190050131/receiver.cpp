#include <iostream>
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <random>
#include <chrono>
#include <time.h>
#include <sys/ioctl.h>

using namespace std;

ofstream receiverfile; // for writing to receiver.txt
/*
        Code for generating random number is taken from https://stackoverflow.com/questions/9878965/rand-between-0-and-1
        Code for ioctl() is taken from https://stackoverflow.com/questions/59326605/how-to-skip-recv-function-when-no-data-is-sent
*/

std::mt19937_64 rng;
std::uniform_real_distribution<double> unif(0, 1); // initialize a uniform distribution between 0 and 1

void seed_uniform_generator()
{
    uint64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count(); // initialize the random number generator with time-dependent seed
    std::seed_seq ss{uint32_t(timeSeed & 0xffffffff), uint32_t(timeSeed >> 32)};
    rng.seed(ss);
    return;
}

double get_random()
{
    return unif(rng); // returns a random number from uniform distr between 0 and 1
}

int setconnection(char *SenderPort, char *ReceivePort) //Exactly same function used in sender.cpp, please refer there for detailed comments
{
    struct addrinfo hints;
    struct addrinfo *sendinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    
    int status = getaddrinfo(NULL, SenderPort, &hints, &sendinfo);
    int socketdescriptor;

    if (status != 0)
    {
        cout << "getaddrinfo error: " << gai_strerror(status) << endl;
        return -1;
    }

    else
    {
        bool connected = false;
        for (struct addrinfo *ptr = sendinfo; ptr != NULL; ptr = ptr->ai_next)
        {
            socketdescriptor = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (socketdescriptor == -1)
            {
                cout << "Unable to get the socket. "
                     << "Now going to next available result" << endl;
                continue;
            }
            int bindresult = bind(socketdescriptor, ptr->ai_addr, ptr->ai_addrlen);
            if (bindresult == -1)
            {
                cout << "Unable to bind to the current port. "
                     << "Moving to the next result now" << endl;
                continue;
            }

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_flags = AI_PASSIVE;

            struct addrinfo *receiverinfo;

            int recv_port_status = getaddrinfo(NULL, ReceivePort, &hints, &receiverinfo);
            bool recv_port_found = false;
            if (recv_port_status != 0)
            {
                cout << "getaddrinfo error  " << gai_strerror(recv_port_status) << endl;
                return -1;
            }
            else
            {
                for (struct addrinfo *recvptr = receiverinfo; recvptr != NULL; recvptr = recvptr->ai_next)
                {
                    int connect_status = connect(socketdescriptor, recvptr->ai_addr, recvptr->ai_addrlen);
                    if (connect_status == -1)
                    {
                        cout << "Connect Failed, Moving to next result" << endl;
                        continue;
                    }
                    else
                    {
                        recv_port_found = true;
                        break;
                    }
                }
            }
            freeaddrinfo(receiverinfo);
            if (recv_port_found = false)
            {
                cout << "All receive ports for this result failed. "
                     << "Going to the next result now!" << endl;
                continue;
            }
            else
            {
                connected = true;
                break;
            }
        }
        if (connected = false)
        {
            cout << "unable to get any good conenction.Aborting the process!" << endl;
            freeaddrinfo(sendinfo);
        }
    }
    freeaddrinfo(sendinfo);
    return socketdescriptor;
}

void send_packet(int socketdescriptor, int AckNumber) // Again, very similar to the sendPakcet function in sender.cpp, please refer to there for detailed comments
{
    stringstream ss;
    ss << "Acknowledgement:" << AckNumber; // as the packet format is "Acknowledgement:<sequenceno>"
    string message = ss.str();

    const char *msg = message.c_str();
    int bytes_sent = send(socketdescriptor, msg, strlen(msg), 0);
    
    cout << "Acknowledgement:" << AckNumber << " sent" << endl;
    receiverfile << "Acknowledgement:" << AckNumber << " sent" << endl;
    return;
}

void receive_packet(int socketdescriptor, int reqdpacketnumber, double packetdrop_prob)
{
    while (true)  // we need to manually end our program, otherwise it keeps running even after all packets are received
    {
        char arr[50] = "";
        int status, bytes_recvd;

        ioctl(socketdescriptor, FIONREAD, &status); //to check if there is any packet waiting to be read        
        if (status > 0)
        {
            //packet waiting to be read
            bytes_recvd = recv(socketdescriptor, arr, 50, 0);
        }
        else
        {
            //no packet waiting to be read
            continue;
        }
        if (bytes_recvd == 0)
        {
            //empty packet received
            continue;
        }
        else
        {
            string s(arr);
            s = s.substr(7, 50);
            std::string::size_type sz; // alias of size_t
            int recvdnumber = stoi(s, &sz); // coverting packet number from string to int

            if (recvdnumber == reqdpacketnumber) // received expected packet
            {
                double randnum = get_random(); // generated a random number between 0 and 1
                if (randnum < packetdrop_prob) // dropping the packet, so not sending ack
                {
                    cout << "Packet:" << recvdnumber << " is received but is dropped" << endl;
                    receiverfile << "Packet:" << recvdnumber << " is received but is dropped" << endl;
                    continue;
                }
                else
                {
                    send_packet(socketdescriptor, reqdpacketnumber + 1); // sending ack for next packet we want
                    reqdpacketnumber++;
                }
            }
            else
            {
                send_packet(socketdescriptor, reqdpacketnumber); // sending ack for the same packet as we did not receive it
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc - 1 != 3) // to ensure usage is proper
    {
        cout << "Wrong usage!" << endl;
        cout << "Usage format: receiver.cpp <ReceiverPort> <SenderPort> <PacketDropProbability>" << endl;
        return 0;
    }
    char *ReceiverPort = argv[1];
    char *SenderPort = argv[2];
    float packetdrop_prob = stod(argv[3]);

    receiverfile.open("receiver.txt"); // writing to receiver.txt

    int socketdescriptor = setconnection(ReceiverPort, SenderPort);
    seed_uniform_generator();

    receive_packet(socketdescriptor, 1, packetdrop_prob); // starting our process by waiting for packet 1

    receiverfile.close();
}