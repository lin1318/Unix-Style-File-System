// FileSystem.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include<iostream>
#include<ctime>
#include<string>
#include<vector>
#include<iomanip>
#include<fstream>
using namespace std;

#define BLOCK_SIZE 1024
#define BLOCK_NUM 16384
#define INODE_SIZE 128
#define INODE_NUM 1024
#define DIRECT_BLOCK_NUM 10
#define MAX_NAME_SIZE 28
#define FILENAME "fs.sys"
#define	FILE 0
#define DIR 1
#define MAX_FILE_SIZE 272384	//I guess (1024/4+10)*1024=256*1024
#define MAX_DIR_NUM 8512		//I guess (10+1024/4)*32=256*32

struct SuperBlock {   //occupy one block(1024 bytes)
	unsigned short totalInodeNum;
	unsigned short totalBlockNum;
	unsigned short freeInodeNum;
	unsigned short freeBlockNum;
	unsigned short superBlockSize;
	int sbStartAddr;
	int inodeBmpStartAddr;
	int blockBmpStartAddr;
	int inodeBlockStartAddr;
	int dataBlockStartAddr;
};
struct inode {		//128byte
	bool type;
	unsigned short inodeID;
	int fileSize;
	int cntItem;	//For dir it means how many items it points to, For file it means how many blocks it points to
	time_t createTime;
	int directBlockAddr[DIRECT_BLOCK_NUM];//0 ".." 1 "." 
	int indirectBlockAddr;				   
};

struct DirItem {		//32 byte
	char itemname[MAX_NAME_SIZE];
	int inodeAddr;
};

fstream fs;
SuperBlock sb;
bool blockBmp[BLOCK_NUM];
bool inodeBmp[INODE_NUM];
int currDirInodeAddr;	//current dir inodeAddr
string currPath;
vector<int> pwdInodeAddr;
//declaration
string pwd();
//helper
int ialloc()  //-1 can't allocate an inode otherwise return the inode start address
{
	for(int i=0;i<INODE_NUM;i++)
		if (inodeBmp[i] == false)
		{
			inodeBmp[i] = true;
			sb.freeInodeNum--;
			return i*INODE_SIZE + sb.inodeBlockStartAddr;
		}
	return -1;
}
int balloc() //-1 can't allocate a block otherwise return the block start address
{
	for(int i=0;i<BLOCK_NUM;i++)
		if (blockBmp[i] == false)
		{
			blockBmp[i] = true;
			sb.freeBlockNum--;
			return i * BLOCK_SIZE + sb.dataBlockStartAddr;
		}
	return -1;
}
void updateInodeBmp()
{
	fs.seekp(sb.inodeBmpStartAddr, ios::beg);	//write inodeBmp
	fs.write((const char*)&inodeBmp, sizeof(inodeBmp));
}
void updateBlockBmp()
{
	fs.seekp(sb.blockBmpStartAddr, ios::beg);	//write blockBmp
	fs.write((const char*)&blockBmp, sizeof(blockBmp));
}
void updateSuperBlock() {
	fs.seekg(sb.sbStartAddr, ios::beg);
	fs.write((const char*)&sb, sizeof(SuperBlock));
}
int getDirAddr(inode currInode,int num)	//return item address
{
	if (num < 320)		//at direct block
	{
		int blocknum = num / 32;
		int offset = num % 32;
		int addr = currInode.directBlockAddr[blocknum] + offset * 32;
		return addr;
	}
	else
	{
		num -= 320;
		int indirectAddr = currInode.indirectBlockAddr;
		int blockAddr[256];
		fs.seekg(indirectAddr, ios::beg);
		fs.read((char*)&blockAddr, sizeof(blockAddr));
		int addrnum = num / 32;
		int offset = num % 32;
		int addr = blockAddr[addrnum] + offset * 32;
		return addr;
	}
}
bool check(string path)  //check whether the path looks like this "/c/"
{
	if (path == "/")
		return false;
	if (path[int(path.size()) - 1] == '/')
		path.pop_back();
	for (int i = 1; i < path.size(); i++)
		if (path[i] == '/')
			return false;	
	return true;
}
int getPathInodeAddr(string path, int currAddr,bool goBottom,bool getFile) //path "/a/b/c/"  currAddr: inode address gobottom: true get c false get b getfile:
{
	string nextpath = "";
	int i;
	if (check(path)&&(!goBottom))
		return currAddr;
	if (path == "/")
		return currAddr;
	for (i = 1; i < path.size(); i++)
		if (path[i] == '/')
			break;
		else
			nextpath += path[i];
	path.erase(path.begin(), path.begin()+i);
	inode currInode;
	fs.seekg(currAddr, ios::beg);
	fs.read((char*)&currInode, sizeof(inode));
	for (int i = 0; i < currInode.cntItem; i++)
	{
		int itemAddr = getDirAddr(currInode, i);
		DirItem tmpItem;
		fs.seekg(itemAddr, ios::beg);
		fs.read((char*)&tmpItem, sizeof(DirItem));
		inode tmpInode;
		fs.seekg(tmpItem.inodeAddr, ios::beg);
		fs.read((char*)&tmpInode, sizeof(inode));
		if (string(tmpItem.itemname) == nextpath)
		{
			if (tmpInode.type == FILE && goBottom&&check(path))
			{
				return tmpItem.inodeAddr;
			}
			if (tmpInode.type == DIR)
			{
				if (goBottom&&check(path) && getFile)
					continue;
				return getPathInodeAddr(path, tmpItem.inodeAddr, goBottom, getFile);
			}
		}
	}
	return -1;
}
void bwrite(int blockAddr)
{
	char block[1024];
	for (int i = 0; i < BLOCK_SIZE; i++)
		block[i] = char(rand() % 26 + 97);
	fs.seekp(blockAddr, ios::beg);
	fs.write((const char*)&block, BLOCK_SIZE);
}
int getBlockAddr(inode currInode, int num)
{
	if (num < DIRECT_BLOCK_NUM)
	{
		return currInode.directBlockAddr[num];
	}
	else
	{
		num -= DIRECT_BLOCK_NUM;
		int blockAddr;
		fs.seekg(currInode.indirectBlockAddr+4*num, ios::beg);
		fs.read((char*)&blockAddr, 4);
		return blockAddr;
	}
}
void bprint(int blockAddr)
{
	char block[BLOCK_SIZE];
	fs.seekg(blockAddr, ios::beg);
	fs.read((char*)&block, BLOCK_SIZE);
	for (int i = 0; i < BLOCK_SIZE; i++)
	{
		if (i % 32 == 0)
			cout << endl;
		cout << block[i];
	}
	return;
}
bool isvalid(string name, int type)
{
	if (type == DIR)
	{
		for (int i = 0; i < name.size(); i++)
			if (!isalnum(name[i]))
				return false;
		return true;
	}
	else
	{
		if (!isalnum(name[0]))
			return false;
		bool haspoint = false;
		for(int i=0;i<name.size();i++)
			if (!isalnum(name[i]))
			{
				if (name[i] == '.' && !haspoint&&i+1!=name.size())
				{
					haspoint = true;
					continue;
				}
				return false;
			}
		return true;
	}
}
bool ispwd(int inodeAddr)
{
	pwd();
	for (int i = 0; i < pwdInodeAddr.size(); i++)
		if (inodeAddr == pwdInodeAddr[i])
			return true;
	return false;
}
string formatPath(string path) //return format path
{
	if (path[0] == '/')
	{
		int lastIndex = int(path.size()) - 1;
		if (path[lastIndex] != '/')
			path.append("/");
		return path;
	}
	else
	{
		path = pwd() + path;
		int lastIndex = int(path.size()) - 1;
		if (path[lastIndex] != '/')
			path.append("/");
		return path;
	}
}
//function
bool addDir(string path, bool flag)	//flag means whether it is the first DIR (has some bugs)
{
	if (flag)	//first dir
	{
		int inodeAddr = ialloc(); //allocate the inode and then update the inodeBmp
		inode tmpinode;			  //add dir named /
		fs.seekg(inodeAddr, ios::beg);
		fs.read((char*)&tmpinode, sizeof(inode));
		tmpinode.type = DIR;		//set inode information
		tmpinode.createTime = time(NULL);
		tmpinode.fileSize = 0;	//DIR has 0 size initially
		tmpinode.cntItem = 2;	//need to store ".." and "." 
		tmpinode.inodeID = (inodeAddr - sb.inodeBlockStartAddr) / (INODE_SIZE);
		currDirInodeAddr = inodeAddr;
		int blockAddr = balloc();
		DirItem tmp;
		tmp.inodeAddr = inodeAddr;//".."
		tmp.itemname[0] = '.', tmp.itemname[1] = '.', tmp.itemname[2] = '\0';
		fs.seekp(blockAddr, ios::beg);
		fs.write((const char*)&tmp, 32);
		tmp.itemname[0] = '.', tmp.itemname[1] = '\0';// "."
		fs.seekp(blockAddr + 32, ios::beg);
		fs.write((const char*)&tmp, 32);
		tmpinode.directBlockAddr[0] = blockAddr;
		fs.seekp(inodeAddr, ios::beg);
		fs.write((const char*)&tmpinode, sizeof(inode));
		updateSuperBlock();
		updateInodeBmp();
		updateBlockBmp();
		return true;
	}
	else
	{
		int lastInodeAddr;
		path = formatPath(path);
		lastInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, false, false); //trace the parent directory
		if (lastInodeAddr == -1)	//whether the path is correct
		{
			cout << "path error" << endl;
			return false;
		}
		string dirname = "";
		for (int i = int(path.size()) - 2; i >= 0; i--)
			if (path[i] != '/')
				dirname += path[i];
			else
				break;
		reverse(dirname.begin(), dirname.end());
		if (dirname.size() >= MAX_NAME_SIZE)  //length limit
		{
			cout << "name is too long" << endl;
			return false;
		}
		if (!isvalid(dirname, DIR))
		{
			cout << "name is not valid" << endl;
			return false;
		}
		//check if the name is the same
		inode lastInode;
		fs.seekg(lastInodeAddr, ios::beg);
		fs.read((char*)&lastInode, sizeof(inode));
		for (int i = 0; i < lastInode.cntItem; i++)
		{
			int dirItemAddr = getDirAddr(lastInode, i);
			DirItem tmpdir;
			fs.seekg(dirItemAddr, ios::beg);
			fs.read((char*)&tmpdir, 32);
			inode tmpinode;
			fs.seekg(tmpdir.inodeAddr, ios::beg);
			fs.read((char*)&tmpinode, sizeof(inode));
			if (strcmp(tmpdir.itemname, dirname.c_str()) == 0)	//same name
			{
				cout << "has the same name" << endl;
				return false;
			}
		}

		if (lastInode.cntItem == MAX_DIR_NUM)	// up to limit of DIR_NUM
		{
			cout << "up to limit of DIR_NUM" << endl;
			return false;
		}
		//allocate inode and block
		if (sb.freeBlockNum == 0 || sb.freeInodeNum == 0)
		{
			cout << "no enough space" << endl;
			return false;
		}

		//ok to add new inode and write
		int blockAddr = balloc();
		int inodeAddr = ialloc();
		inode newinode;
		fs.seekg(inodeAddr, ios::beg);
		fs.read((char*)&newinode, sizeof(inode));
		newinode.type = DIR;
		newinode.cntItem = 2;
		newinode.createTime = time(NULL);
		newinode.fileSize = 0;
		newinode.inodeID = (inodeAddr - sb.inodeBlockStartAddr) / INODE_SIZE;
		newinode.directBlockAddr[0] = blockAddr;
		fs.seekp(inodeAddr, ios::beg);
		fs.write((const char*)&newinode, sizeof(inode));

		//create new dirItem
		DirItem tmpItem;
		fs.seekg(blockAddr, ios::beg);
		fs.read((char*)&tmpItem, 32);
		tmpItem.inodeAddr = lastInodeAddr;
		tmpItem.itemname[0] = '.', tmpItem.itemname[1] = '.', tmpItem.itemname[2] = '\0';
		fs.seekp(blockAddr, ios::beg);
		fs.write((const char*)&tmpItem, 32);

		fs.seekg(blockAddr + 32, ios::beg);
		fs.read((char*)&tmpItem, 32);
		tmpItem.inodeAddr = inodeAddr;
		tmpItem.itemname[0] = '.', tmpItem.itemname[1] = '\0';
		fs.seekp(blockAddr + 32, ios::beg);
		fs.write((const char*)&tmpItem, 32);

		//update last inode
		tmpItem.inodeAddr = inodeAddr;
		memset(tmpItem.itemname, 0, sizeof(tmpItem.itemname));
		strcat(tmpItem.itemname, dirname.c_str());
		lastInode.cntItem++;
		int blockNum = (lastInode.cntItem - 1) / 32;
		int offset = (lastInode.cntItem - 1) % 32;
		int dirItemAddr;
		if((lastInode.cntItem+31)%32!=0)
			dirItemAddr= lastInode.directBlockAddr[blockNum] + offset * 32;
		else
		{
			int additionalBlockAddr = balloc();
			lastInode.directBlockAddr[blockNum] = additionalBlockAddr;
			dirItemAddr = lastInode.directBlockAddr[blockNum] + offset * 32;
		}
		fs.seekp(dirItemAddr, ios::beg);
		fs.write((const char*)&tmpItem, 32);
		fs.seekp(lastInodeAddr, ios::beg);
		fs.write((const char*)&lastInode, sizeof(inode));

		//update information
		updateSuperBlock();
		updateInodeBmp();
		updateBlockBmp();
		return true;
	}
}
void ls()
{
	inode tmpInode;
	fs.seekg(currDirInodeAddr, ios::beg);
	fs.read((char*)&tmpInode, sizeof(inode));
	cout << left;
	for (int i = 2; i < tmpInode.cntItem; i++)
	{
		DirItem tmpItem;
		int dirAddr = getDirAddr(tmpInode, i);
		fs.seekg(dirAddr, ios::beg);
		fs.read((char*)&tmpItem, 32);
		inode subInode;
		fs.seekg(tmpItem.inodeAddr, ios::beg);
		fs.read((char*)&subInode, sizeof(inode));
		struct tm *local;
		local = localtime(&subInode.createTime);
		cout <<setw(35)<<tmpItem.itemname;
		if (subInode.type == FILE)
			cout << setw(10)<<"FILE";
		else
			cout << setw(10)<<"DIR";
		cout <<setw(15)<<to_string(subInode.fileSize)+"B";
		cout << local->tm_year+1900 << "/" << (local->tm_mon + 1) << "/" << (local->tm_mday) << "      ";
		cout << local->tm_hour << ":" << local->tm_min << ":" << local->tm_sec << endl;
	}
	fs.close();
}
string pwd()
{
	string path = "/";
	int inodeAddr = currDirInodeAddr;
	pwdInodeAddr.clear();
	pwdInodeAddr.push_back(inodeAddr);
	while (inodeAddr != sb.inodeBlockStartAddr)
	{
		inode tmp;
		//get last dir
		fs.seekg(inodeAddr, ios::beg);
		fs.read((char*)&tmp, sizeof(inode));
		int blockAddr = tmp.directBlockAddr[0];
		DirItem lastdir;
		fs.seekg(blockAddr, ios::beg);
		fs.read((char*)&lastdir, sizeof(DirItem));
		
		//get current dir's name
		fs.seekg(lastdir.inodeAddr, ios::beg);
		fs.read((char*)&tmp, sizeof(inode));
		int seekItemAddr;
		for (int i = 0; i < tmp.cntItem; i++)
		{
			seekItemAddr = getDirAddr(tmp, i);
			fs.seekg(seekItemAddr, ios::beg);
			DirItem tmpdir;
			fs.read((char*)&tmpdir, 32);
			if (tmpdir.inodeAddr == inodeAddr)
			{
				string name = string(tmpdir.itemname);
				path = "/"+name+path;
				break;
			}
		}
		inodeAddr = lastdir.inodeAddr;
		pwdInodeAddr.push_back(inodeAddr);
	}
	return path;
}
void printSuperBlock()
{
	cout << "Total block number: " << sb.totalBlockNum << endl;
	cout << "Total inode number: " << sb.totalInodeNum << endl;
	cout << "Free block number: " << sb.freeBlockNum << endl;
	cout << "Free inode number: " << sb.freeInodeNum << endl;
}
void format()
{
	char block[BLOCK_SIZE];
	memset(block, 0, sizeof(block));
	for (int i = 0; i < BLOCK_NUM; i++)	//initialize
	{
		fs.seekp(i * BLOCK_SIZE, ios::beg);
		fs.write((const char*)&block, BLOCK_SIZE);
	}
	//initialize SuperBlock
	sb.totalBlockNum = BLOCK_NUM;
	sb.totalInodeNum = INODE_NUM;
	sb.freeBlockNum = BLOCK_NUM;
	sb.freeInodeNum = INODE_NUM;
	sb.sbStartAddr = 0;      // 1 block size
	sb.inodeBmpStartAddr = sb.sbStartAddr + BLOCK_SIZE; // 1024/1024=1 block size
	sb.blockBmpStartAddr = sb.inodeBmpStartAddr + INODE_NUM * 1; // 16384/1024=16 block size
	sb.inodeBlockStartAddr = sb.blockBmpStartAddr + BLOCK_NUM * 1;	// 1024*128/1024=128 block size
	sb.dataBlockStartAddr = sb.inodeBlockStartAddr + INODE_NUM * INODE_SIZE / (BLOCK_SIZE);

	memset(inodeBmp, false, sizeof(inodeBmp));      //initialize inodeBmp

	memset(blockBmp, false, sizeof(blockBmp));		//initialize blockBmp
	int usedBlockNum = 1 + INODE_NUM / BLOCK_SIZE + BLOCK_NUM / BLOCK_SIZE + INODE_NUM * INODE_SIZE / (BLOCK_SIZE);
	for (int i = 0; i < usedBlockNum; i++)
	{
		blockBmp[i] = true;
		sb.freeBlockNum--;
	}
	addDir("/", true);
	currDirInodeAddr = sb.inodeBlockStartAddr;
	updateSuperBlock();
	updateInodeBmp();
	updateBlockBmp();
}
void Load()	//load superblock inode-map block-map
{
	fs.open(FILENAME, ios_base::binary | ios_base::in|ios_base::out);
	if (fs.is_open())
	{
		fs.seekg(sb.sbStartAddr, ios::beg);
		fs.read((char*)&sb,sizeof(SuperBlock));
		fs.seekg(sb.inodeBmpStartAddr, ios::beg);
		fs.read((char*)&inodeBmp, sizeof(inodeBmp));
		fs.seekg(sb.blockBmpStartAddr, ios::beg);
		fs.read((char*)&blockBmp, sizeof(blockBmp));
		currDirInodeAddr = sb.inodeBlockStartAddr;
		fs.close();
	}
	else
	{
		fs.clear();
		fs.open(FILENAME, ios_base::binary | ios_base::out);//create
		fs.close();
		fs.open(FILENAME, ios_base::binary | ios_base::in|ios_base::out); //format
		format();
		fs.close();
	}
}
void cd(string path)
{
	int newInodeAddr;
	path = formatPath(path);
	newInodeAddr= getPathInodeAddr(path, sb.inodeBlockStartAddr,true,false);
	if (newInodeAddr == -1)
	{
		cout << "path error" << endl;
	}
	else
	{
		currDirInodeAddr = newInodeAddr;
	}
}
void updateDirSize(string path,int currInodeAddr,int size)
{
	string nextpath="";
	int i;
	for (i = 1; i < path.size(); i++)
		if (path[i] == '/')
			break;
		else
			nextpath += path[i];
	path.erase(path.begin(), path.begin() + i);
	inode currInode;
	fs.seekg(currInodeAddr, ios::beg);
	fs.read((char*)&currInode, sizeof(inode));
	currInode.fileSize += size;
	fs.seekp(currInodeAddr, ios::beg);
	fs.write((const char*)&currInode, sizeof(inode));
	if (path.empty())
		return;
	for (int i = 0; i < currInode.cntItem; i++)
	{
		int itemAddr = getDirAddr(currInode, i);
		DirItem tmpItem;
		fs.seekg(itemAddr, ios::beg);
		fs.read((char*)&tmpItem, sizeof(DirItem));
		inode tmpInode;
		fs.seekg(tmpItem.inodeAddr, ios::beg);
		fs.read((char*)&tmpInode, sizeof(inode));
		if (string(tmpItem.itemname) == nextpath&&tmpInode.type==DIR)
		{
			updateDirSize(path, tmpItem.inodeAddr,size);
		}
	}
}
bool addFile(string path,int size)
{
	int lastInodeAddr;
	bool isrelated=true;
	if (path[0] == '/')		//absolute path
	{
		isrelated = false;
		lastInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, false,false);
	}
	else                  //relative path
	{
		path = "/" + path;
		lastInodeAddr = getPathInodeAddr(path, currDirInodeAddr, false,false);
	}
	if (lastInodeAddr == -1)	//whether the path is correct
	{
		cout << "path error" << endl;
		return false;
	}
	string filename = "";
	for (int i = int(path.size()) - 1; i >= 0; i--)
		if (path[i] != '/')
			filename += path[i];
		else
			break;
	reverse(filename.begin(), filename.end());
	if (filename.size() >= MAX_NAME_SIZE)  //length limit
	{
		cout << "name is too long" << endl;
		return false;
	}
	if (size > MAX_FILE_SIZE)
	{
		cout << "file is too large" << endl;
		return false;
	}
	if (!isvalid(filename,FILE))
	{
		cout << "name is not valid" << endl;
		return false;
	}
	//check if the name is the same
	inode lastInode;
	fs.seekg(lastInodeAddr, ios::beg);
	fs.read((char*)&lastInode, sizeof(inode));
	for (int i = 0; i < lastInode.cntItem; i++)
	{
		int dirItemAddr = getDirAddr(lastInode, i);
		DirItem tmpdir;
		fs.seekg(dirItemAddr, ios::beg);
		fs.read((char*)&tmpdir, 32);
		inode tmpinode;
		fs.seekg(tmpdir.inodeAddr, ios::beg);
		fs.read((char*)&tmpinode, sizeof(inode));
		if (strcmp(tmpdir.itemname, filename.c_str()) == 0)	//same name and type
		{
			cout << "has the same name" << endl;
			return false;
		}
	}

	if (lastInode.cntItem == MAX_DIR_NUM)	// up to limit of DIR_NUM
	{
		cout << "up to limit of DIR_NUM" << endl;
		return false;
	}
	//allocate inode and block
	if (sb.freeBlockNum<((size+BLOCK_SIZE-1)/BLOCK_SIZE)||sb.freeInodeNum == 0)
	{
		cout << "no enough space" << endl;
		return false;
	}

	//ok to add new inode 
	int inodeAddr = ialloc();
	inode newinode;
	fs.seekg(inodeAddr, ios::beg);
	fs.read((char*)&newinode, sizeof(inode));
	newinode.type = FILE;
	newinode.createTime = time(NULL);
	newinode.fileSize = size;
	newinode.inodeID = (inodeAddr - sb.inodeBlockStartAddr) / INODE_SIZE;

	//write data only solve the directBlockAddr
	int blockcnt = (size + BLOCK_SIZE - 1)/BLOCK_SIZE;
	newinode.cntItem = blockcnt;
	if (blockcnt <= DIRECT_BLOCK_NUM)
	{
		for (int i = 0; i < blockcnt; i++)
		{
			int blockAddr = balloc();
			newinode.directBlockAddr[i] = blockAddr;
			bwrite(blockAddr);
		}
	}
	else
	{
	for (int i = 0; i < DIRECT_BLOCK_NUM; i++)
	{
		int blockAddr = balloc();
		newinode.directBlockAddr[i] = blockAddr;
		bwrite(blockAddr);
	}
	int indirectBlockAddr = balloc();
	int res = blockcnt - DIRECT_BLOCK_NUM;
	newinode.indirectBlockAddr = indirectBlockAddr;
	for (int i = 0; i < res; i++)
	{
		int storeBlockAddr = balloc();
		fs.seekp(indirectBlockAddr + i * 4, ios::beg);
		fs.write((const char*)&storeBlockAddr, 4);
		bwrite(storeBlockAddr);
	}
	}

	fs.seekp(inodeAddr, ios::beg);
	fs.write((const char*)&newinode, sizeof(inode));
	//update last inode

	DirItem tmpItem;
	tmpItem.inodeAddr = inodeAddr;
	memset(tmpItem.itemname, 0, sizeof(tmpItem.itemname));
	strcat(tmpItem.itemname, filename.c_str());
	lastInode.cntItem++;
	int blockNum = (lastInode.cntItem - 1) / 32;
	int offset = (lastInode.cntItem - 1) % 32;
	int dirItemAddr = lastInode.directBlockAddr[blockNum] + offset * 32;
	fs.seekp(dirItemAddr, ios::beg);
	fs.write((const char*)&tmpItem, 32);
	fs.seekp(lastInodeAddr, ios::beg);
	fs.write((const char*)&lastInode, sizeof(inode));
	updateDirSize(path, sb.inodeBlockStartAddr, size);
	updateSuperBlock();
	updateBlockBmp();
	updateInodeBmp();
	return true;
}
void cat(string path)
{
	int currInodeAddr;
	if (path[0] == '/')		//absolute path
	{
		currInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, true,true);
	}
	else                  //relative path
	{
		path = "/" + path;
		currInodeAddr = getPathInodeAddr(path, currDirInodeAddr, true,true);
	}
	if (currInodeAddr == -1)
	{
		cout << "path error" << endl;
	}
	inode file;
	fs.seekg(currInodeAddr, ios::beg);
	fs.read((char*)&file, sizeof(inode));
	char block[BLOCK_SIZE];
	for (int i = 0; i < file.cntItem; i++)
	{
		int blockAddr = getBlockAddr(file, i);
		bprint(blockAddr);
	}
	cout << endl;
}
void rmFile(string path)
{
	int currInodeAddr;
	path = formatPath(path);
	currInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, true,true);
	if (currInodeAddr == -1)
	{
		cout << "path error" << endl;
	}
	inode file;
	fs.seekg(currInodeAddr, ios::beg);
	fs.read((char*)&file, sizeof(inode));
	inodeBmp[file.inodeID] = false;
	sb.freeInodeNum++;
	updateDirSize(path, sb.inodeBlockStartAddr, -file.fileSize);

	int lastInodeAddr;
	lastInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, false,false);

	inode lastDirInode;
	fs.seekg(lastInodeAddr, ios::beg);
	fs.read((char*)&lastDirInode, sizeof(inode));
	for (int i = 0; i < lastDirInode.cntItem; i++)
	{
		int dirAddr = getDirAddr(lastDirInode, i);
		DirItem tmpdir;
		fs.seekg(dirAddr, ios::beg);
		fs.read((char*)&tmpdir, 32);

		if(tmpdir.inodeAddr==currInodeAddr)
		{
			DirItem lastDir;
			int lastAddr = getDirAddr(lastDirInode, lastDirInode.cntItem - 1);
			fs.seekg(lastAddr, ios::beg);
			fs.read((char*)&lastDir, 32);
			fs.seekp(dirAddr, ios::beg);
			fs.write((const char*)&lastDir, 32);
			lastDirInode.cntItem--;
			fs.seekp(lastInodeAddr, ios::beg);
			fs.write((const char*)&lastDirInode, sizeof(inode));
			break;
		}
	}

	for (int i = 0; i < file.cntItem; i++)
	{
		int blockAddr = getBlockAddr(file, i);
		int cnt = (blockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
		blockBmp[cnt] = false;
		sb.freeBlockNum++;
	}
	if (file.cntItem > DIRECT_BLOCK_NUM)
	{
		int cnt = (file.indirectBlockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
		blockBmp[cnt] = false;
		sb.freeBlockNum++;
	}

	updateSuperBlock();
	updateInodeBmp();
	updateBlockBmp();
}
void cp(string src, string dec)
{
	int srcInodeAddr, decInodeAddr;
	src = formatPath(src);
	dec = formatPath(dec);
	srcInodeAddr = getPathInodeAddr(src, sb.inodeBlockStartAddr, true, true);
	decInodeAddr = getPathInodeAddr(dec, sb.inodeBlockStartAddr, true, true);
	if (srcInodeAddr == -1)
	{
		cout << "path error" << endl;
		return;
	}
	if (decInodeAddr != -1)
	{
		rmFile(dec);
	}
	inode srcInode;
	fs.seekg(srcInodeAddr,ios::beg);
	fs.read((char*)&srcInode, sizeof(inode));
	addFile(dec,srcInode.fileSize);
	decInodeAddr = getPathInodeAddr(dec, sb.inodeBlockStartAddr, true, true);
	inode decInode;
	fs.seekg(decInodeAddr, ios::beg);
	fs.read((char*)&decInode, sizeof(inode));
	for (int i = 0; i < decInode.cntItem; i++)
	{
		char blocksrc[BLOCK_SIZE], blockdec[BLOCK_SIZE];
		int srcBlockAddr = getBlockAddr(srcInode, i);
		int decBlockAddr = getBlockAddr(decInode, i);
		fs.seekg(srcBlockAddr, ios::beg);
		fs.read((char*)&blocksrc, BLOCK_SIZE);
		for (int j = 0; j < BLOCK_SIZE; j++)
			blockdec[j] = blocksrc[j];
		fs.seekp(decBlockAddr, ios::beg);
		fs.write((char*)&blockdec, BLOCK_SIZE);
	}
}
void rmDir(string path)
{
	int deleteInodeAddr;
	if (path[0] == '/')		//absolute path
	{
		deleteInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, true, false);
	}
	else                  //relative path
	{
		path = pwd() + path;
		deleteInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, true, false);
	}
	if (deleteInodeAddr == -1)
	{
		cout << "path error" << endl;
		return;
	}
	if (ispwd(deleteInodeAddr))
	{
		cout << "can't delete the current working path" << endl;
		return;
	}
	inode deleteInode;
	fs.seekg(deleteInodeAddr, ios::beg);
	fs.read((char*)&deleteInode, sizeof(inode));
	for (int i = 2; i < deleteInode.cntItem; i++)
	{
		DirItem tmpItem;
		int dirAddr = getDirAddr(deleteInode, i);
		fs.seekg(dirAddr, ios::beg);
		fs.read((char*)&tmpItem, 32);
		inode subItemInode;
		fs.seekg(tmpItem.inodeAddr, ios::beg);
		fs.read((char*)&subItemInode, sizeof(inode));
		if (subItemInode.type == FILE)
		{
			rmFile(path + "/"+string(tmpItem.itemname));
		}
		else
		{
			rmDir(path + "/"+string(tmpItem.itemname));
		}
	}
	//update last inode
	int lastInodeAddr;
	lastInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, false, false);

	inode lastDirInode;
	fs.seekg(lastInodeAddr, ios::beg);
	fs.read((char*)&lastDirInode, sizeof(inode));
	for (int i = 0; i < lastDirInode.cntItem; i++)
	{
		int dirAddr = getDirAddr(lastDirInode, i);
		DirItem tmpdir;
		fs.seekg(dirAddr, ios::beg);
		fs.read((char*)&tmpdir, 32);

		if (tmpdir.inodeAddr ==deleteInodeAddr)
		{
			DirItem lastDir;
			int lastAddr = getDirAddr(lastDirInode, lastDirInode.cntItem - 1);
			fs.seekg(lastAddr, ios::beg);
			fs.read((char*)&lastDir, 32);
			fs.seekp(dirAddr, ios::beg);
			fs.write((const char*)&lastDir, 32);
			lastDirInode.cntItem--;
			fs.seekp(lastInodeAddr, ios::beg);
			fs.write((const char*)&lastDirInode, sizeof(inode));
			break;
		}
	}

	inodeBmp[deleteInode.inodeID] = false;
	sb.freeInodeNum++;
	int blockNum = (deleteInode.cntItem+31) / 32;
	for (int i = 0; i < blockNum; i++)
	{
		int blockAddr = deleteInode.directBlockAddr[i];
		int cnt = (blockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
		blockBmp[cnt] = false;
		sb.freeBlockNum++;
	}
	updateSuperBlock();
	updateInodeBmp();
	updateBlockBmp();
}
int main()
{
	srand(time(NULL));
	Load();   //load superblock
	while (true)
	{
		fs.open(FILENAME, ios_base::binary | ios_base::in | ios_base::out);
		cout << pwd() << "> ";
		string command;
		string parameter;
		cin >> command;
		if (command == "createDir")
		{
			cin >> parameter;
			addDir(parameter, false);
		}
		else if (command == "pwd")
		{
			cout << pwd() << endl;
		}
		else if (command == "format")
		{
			format();
			cout << "successfully format" << endl;
		}
		else if (command == "sum")
		{
			printSuperBlock();
		}
		else if (command == "dir")
		{
			ls();
		}
		else if (command == "changeDir")
		{
			cin >> parameter;
			cd(parameter);
		}
		else if (command == "createFile")
		{
			string dir;
			int size;
			cin >> dir >> size;
			addFile(dir, size * 1024);
		}
		else if (command == "cat")
		{
			string path;
			cin >> path;
			cat(path);
		}
		else if (command == "deleteFile")
		{
			string path;
			cin >> path;
			rmFile(path);
		}
		else if (command == "cp")
		{
			string src, dec;
			cin >> src >> dec;
			cp(src, dec);
		}
		else if (command == "deleteDir")
		{
			string path;
			cin >> path;
			rmDir(path);
		}
		else if (command == "exit")
		{
			fs.close();
			return 0;
		}
		else{
			cout << "Comand not found" << endl;
		}
		fs.close();
	}
	return 0;
}