// sudo gcc LectureSecteur.c -o lectureSecteur -I/usr/include/hfs

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/disk.h>
#include <errno.h>
#include <string.h>
#include <hfs_format.h>

#define VOL_HEADER_START 1024

void readHDD(int descr, unsigned char* secteur, uint startpos, int size);

uint little2big(uint n)
{
	return ( (n >> 24) & 0xff)   | ( (n << 8) & 0xff0000 ) | 
    				 ( (n >> 8) & 0xff00 ) | ( (n << 24) & 0xff000000);
}

void affichageSecteur(unsigned char* secteur, int size)
{
	for (int i = 0; i < size; i+=2)
	{
		printf("%02x%02x ", secteur[i], secteur[i + 1]);
		if (i % 16 == 14) printf("\n"); 
	}
}

void readHDD(int descr, unsigned char* secteur, uint startpos, int size)
{
	lseek( descr, startpos, SEEK_SET );
	read(descr, secteur, size);
	affichageSecteur(secteur, size);
}

void printHFSFileInfo(struct HFSPlusForkData file)
{
	printf("-------- printHFSFileInfo ----------\n");
	for(int i=0;i< 8;i++)
    {
    	printf("blockStart%d: %d\n",i, htonl(file.extents[i].startBlock) );
    	printf("blockCount%d: %d\n",i, htonl(file.extents[i].blockCount) );
    }

    printf("logicalSize: %d\n", htonl(file.logicalSize) );
	printf("totalBlocks: %d\n", htonl(file.totalBlocks) );
	printf("------------------\n");
}

int getBlockSize(struct HFSPlusVolumeHeader *hfs)
{
	return little2big(hfs->blockSize);
}


int main(int argc, char const *argv[])
{
	unsigned char secteur[1024];
	int hdd=0, blockSize=0;

	hdd = open("/dev/rdisk0s2",O_RDONLY);
	if ( hdd < 0 )
	{
		printf("Errorno open: %d  %s\n", errno, strerror(errno));
		return hdd;
	}
	printf("------------- Volume Header ----------------\n");
	readHDD(hdd, secteur, VOL_HEADER_START, 512);

    struct HFSPlusVolumeHeader hfs ;
    memcpy(&hfs, secteur, sizeof(struct HFSPlusVolumeHeader));

    printHFSFileInfo(hfs.catalogFile);

    hfs.blockSize = getBlockSize(&hfs);
    printf("hfs.blockSize:%d\n", hfs.blockSize);
    hfs.catalogFile.extents[0].startBlock = htonl(hfs.catalogFile.extents[0].startBlock);

    uint startBTree = hfs.blockSize * hfs.catalogFile.extents[0].startBlock;

    printf("startFirstBTree=%u, %d\n", startBTree, hfs.blockSize);

    printf("------------- FirstBTree ----------------\n");
    readHDD(hdd, secteur, startBTree, 512);

    struct BTNodeDescriptor btnDesc;
    memcpy(&btnDesc, secteur, sizeof(struct BTNodeDescriptor));
   
    printf("flink=%u\n", htonl(btnDesc.fLink));
    printf("blink=%u\n", htonl(btnDesc.bLink));
    printf("kind=%u\n", btnDesc.kind);
    printf("height=%u\n", btnDesc.height);
    printf("numRecords=%u\n", htons(btnDesc.numRecords));

    struct BTHeaderRec bthRec ;
    memcpy(&bthRec, &secteur[14], sizeof(struct BTHeaderRec));

    printf("rootnode= %u\n", htonl(bthRec.rootNode));
    printf("firstleafnode= %u\n", htonl(bthRec.firstLeafNode));
    printf("attributes= %u\n", htonl(bthRec.attributes));

    uint startFirstLeafNode = htonl(bthRec.firstLeafNode) * hfs.blockSize;

    printf("startFirstLeafNode= %u, %u\n", startFirstLeafNode, hfs.blockSize);
    printf("------------- FirstLeafNode ----------------\n");
    readHDD(hdd, secteur, startFirstLeafNode, 512);



    close(hdd);

	return 0;
}