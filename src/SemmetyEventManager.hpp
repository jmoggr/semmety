// copied from hyprland EventManager
#pragma once
#include <vector>

#include <hyprland/src/defines.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/managers/EventManager.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

struct SemmetyIPCEvent {
	std::string event;
	std::string data;
};

class SemmetyEventManager {
public:
	SemmetyEventManager();
	~SemmetyEventManager();

	void postBarUpdate(std::string updateJson);

private:
	static int onServerEvent(int fd, uint32_t mask, void* data);
	static int onClientEvent(int fd, uint32_t mask, void* data);

	int onServerEvent(int fd, uint32_t mask);
	int onClientEvent(int fd, uint32_t mask);

	struct SClient {
		Hyprutils::OS::CFileDescriptor fd;
		std::vector<SP<std::string>> events;
		wl_event_source* eventSource = nullptr;
	};

	std::vector<SClient>::iterator findClientByFD(int fd);
	std::vector<SClient>::iterator removeClientByFD(int fd);

private:
	Hyprutils::OS::CFileDescriptor m_iSocketFD;
	wl_event_source* m_pEventSource = nullptr;

	std::vector<SClient> m_vClients;
};

inline UP<SemmetyEventManager> g_SemmetyEventManager;
