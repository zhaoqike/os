// fat12.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
using namespace std;
const int dir_cnt=224;
const int dir_sec_cnt=(dir_cnt*32+511)/512;
const int fat_start_sec=1;
const int dir_start_sec=19;
const int data_start_sec=dir_start_sec+dir_sec_cnt;
const int total_clustno = 2880 - 17;


struct dir
{
	char name[11];
	char attr;
	char resv[10];
	short time;
	short data;
	unsigned short fst_clus;
	unsigned long size;
};

int create_floppy(FILE * image);
int get_empty_dir(FILE * image);
int get_two_bytes(FILE * image, int n);
int set_two_bytes(FILE * image, int n, int twobytes);
int get_next_clustno(FILE * image, int n);
int set_next_clustno(FILE * image, int n, int next);
int get_empty_clustno(FILE *image);
int get_lock_empty_clustno(FILE *image);
int add_file(FILE *image, char *file);
int remove_file(FILE *image, char *file);
int add_dir(FILE * image, dir d);
dir create_dir(char* name, int size);
dir get_dir(FILE *image, int n);
int remove_file(FILE *image, char *file);


int _tmain(int argc, char* argv[])
{
	FILE * image=fopen("floppy","wb+");
	int err = create_floppy(image);
	if(err)
	{
		return -1;
	}

	char * files[3]={"river.txt", "dog.txt", "cat.txt"};
	for(int i=0;i<3;i++)
	{
		add_file(image, files[i]);	}
	remove_file(image, "river.txt");
	//remove_file(image, "dog.txt");
	remove_file(image, "cat.txt");
	fclose(image);
	return 0;
}

int create_floppy(FILE * image)
{
	if(image==NULL)
	{
		cout<<"can't create floppy"<<endl;
		return -2;
	}
	char buf[1024];
	memset(buf, 0, sizeof(buf));
	int count=0;
	for(int i=0;i<1440;i++)
	{
		count+=fwrite(buf, sizeof(buf), 1, image);
	}
	if(count!=1440)
	{
		cout<<"can't write original data"<<endl;
		return -1;
	}
	return 0;
}

int get_empty_dir(FILE * image)
{
	dir d;
	for(int i=0;i<dir_cnt;i++)
	{
		d=get_dir(image, i);
		if(d.name[0]==0)
		{
			return i;
		}
	}
	return -1;
}

int get_two_bytes(FILE * image, int n)
{
	int twobytes=0;
	int start=n*3;
	start/=2;
	long offset=fat_start_sec*512+start;
	fseek(image, offset, 0);
	int count = fread(&twobytes, 1, 2, image);
	if(count!=2)
	{
		cout<<"read count error in get_two_bytes "<<count<<endl;
		return -1;
	}
	return twobytes;
}

int set_two_bytes(FILE * image, int n, int twobytes)
{
	int next;
	int start=n*3;
	start/=2;
	long offset=fat_start_sec*512+start;
	fseek(image, offset, 0);
	int count = fwrite(&twobytes, 1, 2, image);
	if(count!=2)
	{
		cout<<"write count error in set_two_bytes "<<count<<endl;
		return -1;
	}
	return 0;
}

int get_next_clustno(FILE * image, int n)
{
	int next = get_two_bytes(image, n);
	//cout<<"twobytes: "<<next<<endl;
	if(n%2) //odd
	{
		next&=0x0000fff0;
		next>>=4;
	}
	else
	{
		next&=0x00000fff;
	}
	//cout<<"next: "<<next<<endl;
	return next;
}

int set_next_clustno(FILE * image, int n, int next)
{
	int twobytes=get_two_bytes(image, n);
	if(n%2) // odd
	{
		twobytes&=0x0000000f;
		next&=0x00000fff;
		next<<=4;
		twobytes|=next;
	}
	else
	{
		twobytes&=0x0000f000;
		next&=0x00000fff;
		twobytes|=next;
	}
	set_two_bytes(image, n, twobytes);
	return 0;
}

int get_empty_clustno(FILE *image)
{
	int next;
	for(int i=2;i<total_clustno;i++)
	{
		next=get_next_clustno(image, i);
		if(next==0)
		{
			return i;
		}
	}
	return -1;
}

int get_lock_empty_clustno(FILE *image)
{
	int empty=get_empty_clustno(image);
	if(empty==-1)
	{
		return -1;
	}
	else
	{
		set_next_clustno(image, empty, 0x0ff8);//lock
		return empty;
	}
}

int add_file(FILE *image, char* file)
{
    char buffer[512];

	FILE * fs = fopen(file, "rb");
	cout<<fs<<endl;
	if(fs==NULL)
	{
		cout<<"read river failed"<<endl;
		return -1;
	}

	dir d=create_dir(file, 0);
	int clustno;
	int next_clustno;
	bool first=true;
	int count;
	int size=0;
	while(count = fread(buffer, 1, 512, fs))
	{
		size+=count;
		cout<<"read count : "<<count<<endl;
		for(int i=0;i<count;i++)
		{
			cout<<buffer[i];
		}
		next_clustno=get_lock_empty_clustno(image);
		cout<<next_clustno<<endl;
		fseek(image, (data_start_sec+next_clustno-2)*512, 0);
		fwrite(buffer, 1, count, image);
		cout<<"data sec : "<<data_start_sec+next_clustno-2<<endl;
		if(first)
		{
			d.fst_clus=next_clustno;
			first=false;
		}
		else
		{
			cout<<"not first "<<clustno<<"  "<<next_clustno<<endl;
			set_next_clustno(image, clustno, next_clustno);
		}
		clustno=next_clustno;
	}
	set_next_clustno(image, clustno, 0xff8);
	d.size=size;
	add_dir(image, d);
	for(int i=0;i<11;i++)
	{
		cout<<d.name[i];
	}
	//cout<<"len : "<<len<<endl;
	//int dir_no=get_empty_dir(image);
	fclose(fs);

	for(int i=0;i<8;i++)
	{
		cout<<get_next_clustno(image, i)<<endl;
	}
	return 0;
}

int create_dir_name(char *dir_name, char *name)
{
	for(int i=0;i<11;i++)
	{
		dir_name[i]=' ';
	}
	for(int i=0;i<8;i++)
	{
		if(name[i]=='.')
		{
			break;
		}
		dir_name[i]=name[i];
	}
	for(int i=0;i<3;i++)
	{
		dir_name[11-1-i]=name[strlen(name)-1-i];
	}
	return 0;
}

dir create_dir(char* name, int size)
{
	dir d;
	create_dir_name(d.name, name);
	d.size=size;
	return d;
}

int write_dir(FILE *image, dir d, int dir_no)
{
	long offset=dir_start_sec*512+dir_no*32;
	cout<<"write dir "<<dir_no<<' '<<offset<<endl;
	fseek(image, offset, 0);
	int count = fwrite(&d, 32, 1, image);
	if(count!=1)
	{
		cout<<"write dir error"<<count<<endl;
		return -1;
	}
	return 0;
}
int add_dir(FILE * image, dir d)
{
	int dir_no=get_empty_dir(image);
	write_dir(image, d, dir_no);
	return 0;
}

dir get_dir(FILE *image, int n)
{
	dir d;
	long offset=dir_start_sec*512+n*32;
	fseek(image, offset, 0);
	int cnt = fread(&d, 32, 1, image);
	if(cnt!=1)
	{
		cout<<"copy dir error"<<endl;
	}
	return d;
}

int find_dir(FILE *image, char *file)
{
	char name[11];
	create_dir_name(name, file);
	dir d;
	for(int i=0;i<dir_cnt;i++)
	{
		d = get_dir(image, i);
		if(strncmp(name, d.name, 11)==0)
		{
			return i;
		}
	}
	return -1;
}

int remove_dir(FILE *image, int dir_no)
{
	dir d;
	char buffer[32];
	memset(buffer, 0, 32);
	memcpy(&d, buffer, 32);
	write_dir(image, d, dir_no);
	return 0;
}


int remove_file(FILE *image, char *file)
{
	int dir_no=find_dir(image, file);
	if(dir_no==-1)
	{
		cout<<"file not exist"<<endl;
		return -1;
	}
	dir d=get_dir(image, dir_no);
	int clustno=d.fst_clus;
	int next_clustno=0;
	while(clustno!=0x0ff8)
	{
		cout<<"clustno "<<clustno<<endl;
		next_clustno = get_next_clustno(image, clustno);
		set_next_clustno(image, clustno, 0);
		clustno=next_clustno;
	}
	remove_dir(image, dir_no);
	for(int i=0;i<8;i++)
	{
		cout<<get_next_clustno(image, i)<<endl;
	}
	return 0;
}