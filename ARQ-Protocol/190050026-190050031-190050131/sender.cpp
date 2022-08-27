#include <iostream>
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/ioctl.h>
using namespace std;

ofstream senderfile; // for writing to sender.txt
/*
        Code for ioctl() is taken from https://stackoverflow.com/questions/59326605/how-to-skip-recv-function-when-no-data-is-sent
*/

int setconnection(char *SenderPort, char *ReceivePort)
{
    struct addrinfo hints;
    struct addrinfo *sendinfo; // this will point to the results of getaddrinfo()

    memset(&hints, 0, sizeof(hints)); // make sure it is empty
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; // as we need to use Data-gram Sockets (UDP)
    hints.ai_flags = AI_PASSIVE; // this tells getaddrinfo() to assign the address of local host to the socket structures.
    
    int status = getaddrinfo(NULL, SenderPort, &hints, &sendinfo);
    int socketdescriptor;

    if (status != 0)
    {
        cout << "getaddrinfo error: " << gai_strerror(status) << endl; //Printing out the errors if there are any
        return -1;
    }

    else  //No error from getaddrinfo()
    {
        bool connected = false; // this variable is updated to true when proper connection is established
        for (struct addrinfo *ptr = sendinfo; ptr != NULL; ptr = ptr->ai_next) // Iterating over the results of getaddrinfo()
        {
            socketdescriptor = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol); // Creating a socket
            if (socketdescriptor == -1)
            {
                cout << "Unable to get the socket. "
                     << "Now going to next available result" << endl;
                continue;
            }
            int bindresult = bind(socketdescriptor, ptr->ai_addr, ptr->ai_addrlen); //Binding the socket to the port
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
            bool recv_port_found = false; //This variable is updated to true when we have found a socket binded to the receiver port
            if (recv_port_status != 0)
            {
                cout << "getaddrinfo error  " << gai_strerror(recv_port_status) << endl;
                return -1;
            }
            else // No error from getaddrinfo()
            {
                for (struct addrinfo *recvptr = receiverinfo; recvptr != NULL; recvptr = recvptr->ai_next)
                {
                    int connect_status = connect(socketdescriptor, recvptr->ai_addr, recvptr->ai_addrlen); // trying to establish a connection
                    if (connect_status == -1)
                    {
                        cout << "Connect Failed, Moving to next result" << endl;
                        continue;
                    }
                    else
                    {
                        recv_port_found = true; // found a receiver port and established connection
                        break;
                    }
                }
            }
            freeaddrinfo(receiverinfo); // freeing data in receiverinfo
            if (recv_port_found = false)
            {
                cout << "All receive ports for this result failed. "
                     << "Going to the next result now!" << endl;
                continue;
            }
            else
            {
                connected = true; // established connection between sender and receiver ports
                break;
            }
        }
        if (connected = false)
        {
            cout << "Unable to get any good conenction. Aborting the process!" << endl;
            freeaddrinfo(sendinfo); // freeing data in sendinfo
            return -1;
        }
    }
    freeaddrinfo(sendinfo);
    return socketdescriptor; // socketdescriptor of the socket binded to the sender port
}

void sendPacket(int socketdescriptor, int i)
{
    stringstream ss;
    ss << "Packet:" << i;  // as the packet format is "Packet:<sequenceno>"
    string message = ss.str();
    
    const char *msg = message.c_str();
    int bytes_sent = send(socketdescriptor, msg, strlen(msg), 0); // sent the message to the port connected with sender's port
    
    cout << "Packet:" << i << " sent" << endl; // printing out the packet sent to the terminal
    senderfile << "Packet:" << i << " sent" << endl; // writing the same to sender.txt
    return;
}

bool receivepacket(int socketdescriptor, int expected_number, double RetransmissionTimer)
{
    time_t start;
    start = time(NULL); //time when we started the timer
    int status, bytes_recvd;

    while (difftime(time(NULL), start) < double(RetransmissionTimer)) //to end the process if the retransmission timer expires
    {
        char arr[50] = "";
        ioctl(socketdescriptor, FIONREAD, &status); //to check if there is any packet waiting to be read

        if (status > 0)
        {
            //packet waiting to be read
            bytes_recvd = recv(socketdescriptor, arr, 50, 0); //message received is stored in arr
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
            string s(arr); //copying message from char array arr to string s
            s = s.substr(16, 50); //50 is deliberately used . because string size will be less than 50;
            std::string::size_type sz; // alias of size_t
            int recvdnumber = stoi(s, &sz); //converting packet number from string to int 
            if (recvdnumber == expected_number)
            {
                return true; //received the expected ack
            }
            else
            {
                continue;  //ignoring the ack if it is not what we expected
            }
        }
    }
    cout << "Retransmission timer expired while waiting for Acknowledgement:" << expected_number << endl;
    senderfile << "Retransmission timer expired while waiting for Acknowledgement:" << expected_number << endl;
    return false; // Timer expired but did not receive expected ack
}

int main(int argc, char *argv[])
{
    if (argc - 1 != 4) // to ensure usage is proper
    {
        cout << "Wrong usage!" << endl;
        cout << "Usage format: sender.cpp <SenderPort> <ReceiverPort> <RetransmissionTimer> <NoOfPacketsToBeSent>" << endl;
        return 0;
    }

    char *SenderPort = argv[1]; // sender port
    char *ReceiverPort = argv[2]; // receiver port
    double RetransmissionTimer = stod(argv[3]); // retransmission timer
    int NumPackets = atoi(argv[4]); // no.of packets to be sent

    senderfile.open("sender.txt"); // writing to sender.txt

    int socketdescriptor = setconnection(SenderPort, ReceiverPort); 
    
    for (int i = 1; i <= NumPackets; i++)
    {
        sendPacket(socketdescriptor, i); // sending packet number i
        bool recvd = receivepacket(socketdescriptor, i + 1, RetransmissionTimer); //receiving ack for packet i+1
        if (!recvd)
        {
            // if ack for i+1 is not received repeat send and receive for the same i
            i--; // decrement i here as it is again incremented after this run of for loop
            continue;
        }
    }
    senderfile.close(); // to avoid errors
}