#include <iostream>
#include <enet/enet.h>
#include <string>
#include <thread>
#include <mutex>

const int MAX_CONNECTIONS = 1;
const int NUM_CHANNELS = 1;
const std::string SERVER_IP = "localhost";
const int SERVER_PORT = 7777;
const int TIMEOUT_LIMIT_MS = 1000;
std::string clientName = "Client"; // can put your name here
bool clientIsRunning = true;

// handleEventsThread
bool isWaitingForInput = true;
std::mutex isWaitingForInputMutex;

bool handleEventsThreadIsRunning = true;
std::mutex handleEventsThreadMutex;
void notifyHandleEventsThread()
{
	handleEventsThreadMutex.lock();
	handleEventsThreadIsRunning = false;
	handleEventsThreadMutex.unlock();
}

int main()
{
	if (enet_initialize() != 0)
	{
		std::cerr << "An error occurred while initializing ENet!\n";
		return 1;
	}
	atexit(enet_deinitialize);

	ENetHost* client;
	client = enet_host_create(NULL, MAX_CONNECTIONS, NUM_CHANNELS, 0, 0); // 0, 0 means no bandwidth limits

	if (client == NULL)
	{
		std::cout << "ENet failed to create client!\n";
		return -1;
	}

	ENetAddress address;
	enet_address_set_host(&address, SERVER_IP.c_str());
	address.port = SERVER_PORT;

	ENetPeer* peer;
	peer = enet_host_connect(client, &address, NUM_CHANNELS, 0); // 0 is a user-supplied data (0 means no data)

	if (peer == NULL)
	{
		std::cout << "No available peers for initiating an ENet connection (no server available)!\n";
		return -1;
	}

	ENetEvent eNetEvent;
	if (enet_host_service(client, &eNetEvent, TIMEOUT_LIMIT_MS) > 0 && eNetEvent.type == ENET_EVENT_TYPE_CONNECT)
	{
		std::cout << "Connection to " << SERVER_IP << ":" << SERVER_PORT << " succeeded.\n";
	}
	else
	{
		enet_peer_reset(peer);
		std::cout << "Connection to " << SERVER_IP << ":" << SERVER_PORT << " failed!\n";
		return -1;
	}

	if (enet_host_service(client, &eNetEvent, TIMEOUT_LIMIT_MS) > 0)
	{
		unsigned int clientID = *reinterpret_cast<unsigned int*>(eNetEvent.packet->data);
		clientName += std::to_string(clientID);
		std::cout << "Client Name: " << clientName << '\n';
		enet_packet_destroy(eNetEvent.packet);
	}
	else
	{
		enet_peer_reset(peer);
		std::cout << "Connection to " << SERVER_IP << ":" << SERVER_PORT << " did not provide a client name!\n";
		return -1;
	}

	// packet = enet_packet_create("Hello, server!", strlen("Hello, server!") + 1, ENET_PACKET_FLAG_RELIABLE);

	// enet_peer_send(peer, 0, packet);

	std::thread handleEventsThread([&]()
	{
		ENetPacket* receivedPacket;

		handleEventsThreadMutex.lock();
		while (handleEventsThreadIsRunning)
		{
			handleEventsThreadMutex.unlock();

			while (enet_host_service(client, &eNetEvent, TIMEOUT_LIMIT_MS) > 0)
			{
				switch (eNetEvent.type)
				{
				case ENET_EVENT_TYPE_RECEIVE:

					isWaitingForInputMutex.lock();
					if (isWaitingForInput)
					{
						// std::cout << "\033[1A" << std::flush; // move cursor up one line
						std::cout << "\033[2K" << std::flush; // clear the line
						std::cout << "\r\t\t\t" << std::flush; // move cursor to the beginning of the line and do some tabs

						std::cout << eNetEvent.packet->data << '\n';

						std::cout << clientName << ": ";
					}
					else
					{
						std::cout << "\t\t\t" << eNetEvent.packet->data << '\n';
					}
					isWaitingForInputMutex.unlock();

					enet_packet_destroy(eNetEvent.packet);
					break;
				default:
					break;
				}
			}

			handleEventsThreadMutex.lock();
		}
		handleEventsThreadMutex.unlock();
	
	});

	ENetPacket* sentPacket;
	while (clientIsRunning)
	{
		isWaitingForInputMutex.lock();
		isWaitingForInput = true;
		isWaitingForInputMutex.unlock();

		std::cout << clientName << ": ";

		std::string messageToSend;
		std::getline(std::cin, messageToSend);

		isWaitingForInputMutex.lock();
		isWaitingForInput = false;
		isWaitingForInputMutex.unlock();

		if (messageToSend == "exit")
		{
			clientIsRunning = false;
		}

		messageToSend = clientName + ": " + messageToSend;

		sentPacket = enet_packet_create(messageToSend.c_str(), messageToSend.size() + 1, ENET_PACKET_FLAG_RELIABLE);
		enet_peer_send(peer, 0, sentPacket);
		// enet_host_flush(client); // not necessary, but useful if you want to send the packet immediately
	}

	notifyHandleEventsThread();
	handleEventsThread.join();

	enet_host_flush(client);
	enet_peer_disconnect(peer, 0);

	while (enet_host_service(client, &eNetEvent, TIMEOUT_LIMIT_MS) > 0)
	{
		switch (eNetEvent.type)
		{
		case ENET_EVENT_TYPE_RECEIVE:
			std::cout << eNetEvent.packet->data << '\n';
			enet_packet_destroy(eNetEvent.packet);
			break;
		default:
			break;
		}
	}

	enet_host_destroy(client);

	return 0;
}
