// TODO Implement logging over network
#include "debugScreen.h"

#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/rng.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/rtc.h>
#include <psp2/sysmodule.h>

#define printf psvDebugScreenPrintf

#include <config.h>
#include <syncer.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int power_thread(SceSize args, void *argp);

int tcp_connection = -1;
int open_file = -1;
static volatile int g_power_lock;

static const int PORT = 9483;

int main(void)
{
	g_power_lock = 1;
	SceUID power_thread_uid = sceKernelCreateThread(
	    "power_thread", &power_thread, 0x10000100, 0x40000, 0, 0, NULL);
	if (power_thread >= 0) {
		sceKernelStartThread(power_thread_uid, 0, NULL);
	}

	psvDebugScreenInit();
	psvDebugScreenClear(0);

	config_data *config = config_parse("ux0:/data/plybck.cfg");

	static char net_mem[1 * 1024 * 1024];
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	sceNetInit(&(SceNetInitParam){net_mem, sizeof(net_mem), 0});
	tcp_connection = socket(PF_INET, SOCK_STREAM, 0);
	struct hostent *host;
	if ((host = gethostbyname(config->server)) == NULL) {
		clbk_show_error("Couldn't resove server");
	}
	int cret = connect(tcp_connection,
			   (const struct sockaddr *)&((struct sockaddr_in){
			       .sin_family = AF_INET,
			       .sin_port = htons(PORT),
			       .sin_addr.s_addr = *(long *)(host->h_addr)}),
			   sizeof(struct sockaddr_in));

	if (tcp_connection == -1 || cret == -1)
		clbk_show_error("Connect to server failed");
	LOG("Starting sync\n");
	syncer_run(config, "vita", "0.1");
	close(open_file);
	close(tcp_connection);
	sceNetTerm();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	g_power_lock = 0;
	sceKernelDelayThread(~0);
}

static int power_thread(SceSize args, void *argp)
{
	for (;;) {
		int lock;
		__atomic_load(&g_power_lock, &lock, __ATOMIC_SEQ_CST);
		if (lock > 0) {
			sceKernelPowerTick(
			    SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
		}

		sceKernelDelayThread(1 * 1000 * 1000);
	}
	return 0;
}
// Implementation should open tcp connection befor calling syncer_run
// If connection is closed prematurely, display error
void clbk_send(uint8_t *data, uint32_t length)
{
	write(tcp_connection, data, length);
}

uint32_t clbk_receive(uint8_t *data, uint32_t length)
{
	int ret = read(tcp_connection, data, length);
	if (ret == -1)
		clbk_show_error("Read error");
	return ret;
}

// Open file for reading, close previous
int clbk_open(char *path)
{
	printf("Opening %s\n", path);
	if (open_file != -1)
		close(open_file);
	open_file = open(path, O_RDONLY);
	if (open_file == -1) {
	}
	return (open_file == -1) ? open_file : 0;
}

uint32_t clbk_read(uint8_t *data, uint32_t length)
{
	return read(open_file, data, length);
}

struct dir_desc {
	DIR *dir;
	char *path;
};

// The path is absolute
void *clbk_open_dir(char *path)
{
	struct dir_desc *dird = malloc(sizeof(struct dir_desc));
	dird->dir = opendir(path);
	dird->path = malloc(strlen(path) + 1);
	memcpy(dird->path, path, strlen(path) + 1);
	if (dird->dir == NULL) {
		free(dird);
		printf("Failed to open %s with error code %x\n", path, errno);
		return NULL;
	}
	return dird;
}

void clbk_close_dir(void *dird_void)
{
	struct dir_desc *dird = (struct dir_desc *)dird_void;
	closedir(dird->dir);
	free(dird->path);
	free(dird);
}

// allocate fodlre_entry statically
struct dir_entry *clbk_read_dir(void *dird_void)
{
	struct dir_desc *dird = (struct dir_desc *)dird_void;
	static struct dir_entry dire;
	struct dirent *dirret = readdir(dird->dir);
	if (dirret == NULL)
		return NULL;
	if (SCE_S_ISREG(dirret->d_stat.st_mode)) {
		dire.dir = false;
		dire.size = dirret->d_stat.st_size;

		sceRtcGetTime64_t(&dirret->d_stat.st_mtime, &dire.mtime);
	} else if (SCE_S_ISDIR(dirret->d_stat.st_mode)) {
		dire.dir = true;
		dire.size = 0;
		dire.mtime = 0;
	} else {
		clbk_show_error("Invalid file in dir\n");
	}
	dire.name = dirret->d_name;
	return &dire;
}

uint32_t clbk_file_size(char *path)
{
	struct stat statbuffer;
	if (stat(path, &statbuffer) == -1)
		clbk_show_error("Couldn't stat file");

	return statbuffer.st_size;
}

// Exit in this function
void clbk_show_error(char *msg)
{
	printf("%s\n", msg);
	if (open_file != -1)
		close(open_file);
	if (tcp_connection != -1)
		close(tcp_connection);
	sceNetTerm();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	g_power_lock = 0;
	sceKernelDelayThread(~0);
	exit(1);
}

void clbk_show_status(char *status)
{
	printf("%s", status);
}

int clbk_config_entry(char *key, char *val)
{
	(void)(key);
	(void)(val);
	return -1;
}

void clbk_delay(uint8_t ms)
{
	sceKernelDelayThread(ms * 1000);
}

void clbk_get_random(uint8_t *data, uint8_t len)
{
	if (sceKernelGetRandomNumber(data, len) != 0)
		clbk_show_error("Couldn't get random number");
}
