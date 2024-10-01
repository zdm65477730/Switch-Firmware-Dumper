#include <switch.h>
#include <string>
#include <dirent.h>
#include <experimental/filesystem>
//#include <sys/stat.h>
#include "dump.h"
#include "cons.h"
#include "dir.h"

namespace fs = std::experimental::filesystem;
using namespace std;

dumpArgs* dumpArgsCreate(console* c, bool* status) {
	dumpArgs* ret = new dumpArgs;
	ret->c = c;
	ret->thFin = status;
	return ret;
}

void deleteDirectoryContents(const std::string& dir_path) {
	for (const auto& entry : fs::directory_iterator(dir_path))
		fs::remove(entry.path());
}

int countEntriesInDir(const char* dirname) {
	int files = 0;
	dirent* d;
	DIR* dir = opendir(dirname);
	if (dir == NULL) return 0;
	while ((d = readdir(dir)) != NULL) files++;
	closedir(dir);
	return files;
}

void daybreak() {	
	unsigned long min = 5633; //size in bytes
	struct dirent* ent;
	DIR* dir = opendir("sdmc:/Dumped-Firmware");
	while ((ent = readdir(dir)) != NULL) {
		FILE *f = NULL;	
		//this code block is to give the files in the dir a full path - or we get a crash.
		string partpath = "sdmc:/Dumped-Firmware/";
		const char* name = ent->d_name;
		partpath += name;
		const char *C = partpath.c_str();
		//end of code block
		
		f = fopen(C,"rb");
		fseek(f, 0, SEEK_END);
		unsigned long len = (unsigned long)ftell(f);
		fclose(f);

		if (len < min){
			string newfile = C;
			string s2 = "cnmt.nca";
			
			if (strstr(newfile.c_str(),s2.c_str())) {
			//
			} else {
					string renamed = newfile.replace(newfile.find("nca"), sizeof("nca") - 1, "cnmt.nca");
					const char *D = renamed.c_str();
					rename(C, D);
			}
		}
	}
	closedir(dir);
}

void deletedump() {
	//remove the dumped files
	fs::path p = ("sdmc:/Dumped-Firmware");
	deleteDirectoryContents(p);
}

void dumpArgsDestroy(dumpArgs* a) {
	delete a;
}

void dumpThread(void* arg) {
	dumpArgs* a = (dumpArgs*)arg;
	console* c = a->c;
	
	DIR* dir = opendir("sdmc:/Dumped-Firmware");
	if (!dir) {
			FsFileSystem sys;
		if (R_SUCCEEDED(fsOpenBisFileSystem(&sys, FsBisPartitionId_System, ""))) {
			fsdevMountDevice("sys", sys);
			c->out("转储开始。");
			c->nl();

			//Del first for safety
			//delDir("sdmc:/Update/", false, c);
			mkdir("sdmc:/Dumped-Firmware", 777);

			//Copy whole contents folder
			copyDirToDir("sys:/Contents/registered/", "sdmc:/Dumped-Firmware/", c);

			fsdevUnmountDevice("sys");
			c->nl();
			c->out("当前NAND固件文件已成功转储到SD卡。");
			c->nl();
			
			c->out("为Daybreak重命名文件，^请稍候^，这需要一点时间。");
			c->nl();
			daybreak();
			c->out("固件文件已重命名为与Daybreak兼容。转储序列现已完成&祝您今天愉快&");
			c->nl();
		} else {
			c->out("*打开系统分区失败！*");
			c->nl();
		}
	}
	closedir(dir);
	*a->thFin = true;
}

void cleanThread(void *arg) {
	dumpArgs* a = (dumpArgs*)arg;
	console* c = a->c;
	
	DIR* dir = opendir("sdmc:/Dumped-Firmware");
	if (dir) {
			closedir(dir);
			c->clear();
			c->out("正在从您的SD卡中删除当前的固件转储，请稍候。");
			c->nl();
			deletedump();
			rmdir("sdmc:/Dumped-Firmware");
			c->out("^已删除转储固件！^");
			c->nl();
	} else {
			c->clear();
			c->out("SD卡中^不^存在转储的固件！");
			c->nl();
	}
	*a->thFin = true;
}

void cleanPending(void *arg) {
	dumpArgs* a = (dumpArgs*)arg;
	console* c = a->c;
	
	int filenumber = 0;
	
	FsFileSystem sys;
	if (R_SUCCEEDED(fsOpenBisFileSystem(&sys, FsBisPartitionId_System, ""))) {
			fsdevMountDevice("sys", sys);
			DIR* dir = opendir("sys:/Contents/placehld/");
			if (dir) {
					filenumber = countEntriesInDir("sys:/Contents/placehld/");
					if (filenumber != 0) {
						string str = to_string(filenumber);
						c->out("试图删除NAND里待更新的nca^" + str + "^ - &请稍候！&");
						c->nl();
						deleteDirectoryContents("sys:/Contents/placehld");
						closedir(dir);
						rmdir("sys:/Contents/placehld");
						c->out("^待更新的nca已成功从您的NAND中删除。^");
						c->nl();
					} else {
						string str = to_string(filenumber);
						c->out("占位文件夹里有^" + str + "^个文件 - 没有要删除的NCA文件！");
						c->nl();
					}
				} else {
					c->clear();
					c->out("在NAND里未找到待更新的nca文件。");
					c->nl();
				}
				fsdevUnmountDevice("sys");
		} else {
				c->clear();
				c->out("*打开系统分区失败！*");
				c->nl();
		}
		
		*a->thFin = true;
}