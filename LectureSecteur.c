// sudo gcc LectureSecteur.c -o lectureSecteur -I/usr/include/hfs

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/disk.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <hfs_format.h>

#define VOL_HEADER_START 1024
#define NODE_DESCR_SIZE  14

typedef u_int32_t HFSCatalogNodeID;

struct BTreePointerRecord {
	u_int16_t keyLength;
	u_int32_t parentId;
	u_int16_t length;
  	unsigned char *key;
  	u_int32_t pointer;
};

int hdd=0;

//refaire ca en mieux!!!
void printUTFasChar(uint length, unsigned char* key)
{
	printf("Key: ");
	for (int i = 0; i < length; i++)
	{
		// if (i % 2 == 1)
			printf("%c", key[i]);
	}
	printf("\n");
}

u_int16_t readUShort(int seek, unsigned char* block)
{
	return htons((block[seek + 1] << 8) + block[seek]);
}

u_int32_t readULong(int seek, unsigned char* block)
{
	return htonl((block[seek + 3] << 24) + (block[seek + 2] << 16) + (block[seek + 1] << 8) + block[seek]);
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
	read( descr, secteur, size );
	// affichageSecteur(secteur, size);
}

uint getNodeOffsetInCat(uint node, u_int16_t size, uint startBTree)
{
	return node * size + startBTree;
}

void fillNodeDescriptor(struct BTNodeDescriptor *desc, unsigned char* sect, int print)
{
	memcpy(desc, sect, sizeof(struct BTNodeDescriptor));

	desc->fLink = htonl(desc->fLink);
	desc->bLink = htonl(desc->bLink);
	desc->numRecords = htons(desc->numRecords);
	desc->reserved = htons(desc->reserved);

	if(print > 0)
	{
		printf("flink=%u\n", desc->fLink);
	    printf("blink=%u\n", desc->bLink);
	    printf("kind=%d\n", desc->kind);
	    printf("height=%u\n", desc->height);
	    printf("numRecords=%u\n", desc->numRecords);

	}
}

struct BTreePointerRecord* getPointerRecs(int nbRec, uint* offsets, unsigned char* nodeBlock)
{
	struct BTreePointerRecord *pointRecs = malloc(sizeof(struct BTreePointerRecord) * nbRec);
	for (int i = 0; i < nbRec; i++)
	{
		pointRecs[i].keyLength = readUShort(offsets[i], nodeBlock);
		printf("Pointer record %d has key length %u.\n", i + 1, pointRecs[i].keyLength);
		int seek = offsets[i] + 2; // tete de lecture
		pointRecs[i].parentId = readULong(seek, nodeBlock);
		printf("ParentId: %u\n", pointRecs[i].parentId);
		seek += 4;
		pointRecs[i].length = readUShort(seek, nodeBlock) * 2;
		seek += 2;
		printf("Pointer record %d has node name length %u.\n", i+1, pointRecs[i].length);
		unsigned char *key = malloc(sizeof(unsigned char) * pointRecs[i].length);
		for (int j = 0; j < pointRecs[i].length; j++)
		{
			key[j] = nodeBlock[seek + j];
			// printf("Character %d is '%c'", j, key[j]);
		}
		printUTFasChar(pointRecs[i].length, key);
		pointRecs[i].key = key;
		seek += pointRecs[i].length;
		pointRecs[i].pointer = readULong(seek, nodeBlock);
		printf("Pointer record %d points to %u.\n", i + 1, pointRecs[i].pointer);
	}
	return pointRecs;
}

uint* parseRecordOffsets(int nbRec, unsigned char* secteur, int nodeSize)
{
	int nbOffsets = nbRec + 1;
	uint *recOffsets = malloc(sizeof(uint) * nbOffsets);
	printf("nb of offsets:%d\n", nbOffsets);
	printf("This node record offsets: ");
	for (int i = 0, j = nodeSize-1; i < nbOffsets; i++, j -= 2)
	{
		recOffsets[i] = (secteur[j-1] << 8) + secteur[j];
		if (i < nbRec)
			printf("%u, ", recOffsets[i]);
		else
			printf("\nfree space: %u\n", recOffsets[i]);
	}
	return recOffsets;
}

void readHeaderRecord(struct BTHeaderRec *hRec, uint offset)
{
	unsigned char secteur[1024];
	readHDD(hdd, secteur, offset , 1024);


	memcpy(hRec, secteur+14, sizeof(struct BTHeaderRec));
	hRec->treeDepth = htons(hRec->treeDepth);
	hRec->rootNode = htonl(hRec->rootNode);
	hRec->leafRecords = htonl(hRec->leafRecords);
	hRec->firstLeafNode = htonl(hRec->firstLeafNode);
	hRec->lastLeafNode = htonl(hRec->lastLeafNode);
	hRec->nodeSize = htons(hRec->nodeSize);
	hRec->maxKeyLength = htons(hRec->maxKeyLength);
	hRec->totalNodes = htonl(hRec->totalNodes);
	hRec->freeNodes = htonl(hRec->freeNodes);
	hRec->reserved1 = htons(hRec->reserved1);
	hRec->clumpSize = htonl(hRec->clumpSize);
	hRec->attributes = htonl(hRec->attributes);
}

void readLeafNode()
{
	printf("readLeafNode\n");
	return;
}

void readIndexRecord(unsigned char* sect, uint offset, HFSCatalogNodeID cnid, uint* pointer)
{
	printf("reading index record\n");
	printf("offset: %u\n", offset);
	printf("sect: %s\n", sect);

	struct BTreePointerRecord pointRec;
	memcpy(&pointRec, sect + offset, sizeof(struct BTreePointerRecord));
	*pointer = htonl(pointRec.pointer);
	printf("parentId:%u\n", htonl(pointRec.parentId));
	printf("pointer = %08.8X\n", *pointer);



}

void readIndexNode(HFSCatalogNodeID cnid, uint *node, u_int16_t nodesize, uint offset, u_int8_t *height)
{
	unsigned char secteur[nodesize];
	readHDD(hdd, secteur, offset, nodesize);


	struct BTNodeDescriptor desc;

	fillNodeDescriptor(&desc, secteur, 0);

	uint *recOffsets = parseRecordOffsets(desc.numRecords, secteur, nodesize);
	printf("numRecords:%u, recOffsets[2]=%u\n", desc.numRecords, recOffsets[2]);
	// on alloue la memoire pour nos records du node
	
	for(int i = 0; i < desc.numRecords; i++)
	{
		readIndexRecord(secteur, recOffsets[i], cnid, node);
	}
	*height = desc.height;

	return;
}

void getLeafNode(HFSCatalogNodeID cnid, uint startBTree)
{
	struct BTHeaderRec header;
	readHeaderRecord(&header, startBTree);

	u_int8_t height = header.treeDepth;
	uint node  = header.rootNode;
	u_int16_t nodesize = header.nodeSize;

	printf("height:%u, node:%u, nodesize:%u\n", header.treeDepth, header.rootNode, header.nodeSize);
	while(height > 1)
	{
		printf("height =%d\n", height);
		uint offset = getNodeOffsetInCat(node, nodesize, startBTree);
		readIndexNode(cnid, &node, nodesize, offset, &height);
		height = 1;
	}
	readLeafNode();
}


uint little2big(uint n)
{
	return ( (n >> 24) & 0xff)   | ( (n << 8) & 0xff0000 ) | 
    				 ( (n >> 8) & 0xff00 ) | ( (n << 24) & 0xff000000);
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

void fillBTHeaderRec(struct BTHeaderRec *hRec, unsigned char* sect, int print)
{
	memcpy(hRec, sect, sizeof(struct BTHeaderRec));
	hRec->treeDepth = htons(hRec->treeDepth);
	hRec->rootNode = htonl(hRec->rootNode);
	hRec->leafRecords = htonl(hRec->leafRecords);
	hRec->firstLeafNode = htonl(hRec->firstLeafNode);
	hRec->lastLeafNode = htonl(hRec->lastLeafNode);
	hRec->nodeSize = htons(hRec->nodeSize);
	hRec->maxKeyLength = htons(hRec->maxKeyLength);
	hRec->totalNodes = htonl(hRec->totalNodes);
	hRec->freeNodes = htonl(hRec->freeNodes);
	hRec->reserved1 = htons(hRec->reserved1);
	hRec->clumpSize = htonl(hRec->clumpSize);
	hRec->attributes = htonl(hRec->attributes);
	
	if(print > 0)
	{
		printf("----- BT HEADER RECORD -----\n");
		printf("tree depth = %d\n", hRec->treeDepth);
		printf("rootNode = %d\n", hRec->rootNode);
		printf("leafRecords = %d\n", hRec->leafRecords);
		printf("firstLeafNode = %d\n", hRec->firstLeafNode);
		printf("lastLeafNode = %d\n", hRec->lastLeafNode);
		printf("nodeSize = %d\n", hRec->nodeSize);
		printf("maxKeyLength = %d\n", hRec->maxKeyLength);
		printf("totalNodes = %d\n", hRec->totalNodes);
		printf("freeNodes = %d\n", hRec->freeNodes);
		printf("clumpSize = %d\n", hRec->clumpSize);
		printf("attributes = %d\n", hRec->attributes);
	}
}


void fillCatFolderRec(struct HFSPlusCatalogFolder *recPtr, unsigned char* sect, int print)
{
	memcpy(recPtr, sect, sizeof(struct HFSPlusCatalogFolder));
	recPtr->recordType = htons(recPtr->recordType);
	recPtr->flags = htons(recPtr->flags);
	recPtr->valence = htonl(recPtr->valence);
	recPtr->folderID = htonl(recPtr->folderID);
	recPtr->createDate = htonl(recPtr->createDate);
	recPtr->contentModDate = htonl(recPtr->contentModDate);
	recPtr->attributeModDate = htons(recPtr->attributeModDate);
	recPtr->accessDate = htonl(recPtr->accessDate);
	recPtr->backupDate = htonl(recPtr->backupDate);
	recPtr->textEncoding = htonl(recPtr->textEncoding);
	//recPtr->reserved = htonl(recPtr->reserved);

	if(print > 0)
	{
		printf("recordType= %d\n", recPtr->recordType);
		printf("flags= %u\n", recPtr->flags);
		printf("nb file/folder = %u\n", recPtr->valence);
		printf("createDate= %u\n", recPtr->createDate);
		printf("contentModDate= %u\n", recPtr->contentModDate);
		printf("accessDate= %u\n", recPtr->accessDate);
	}
}



int getFolderRecStartPos(short int keyLen)
{
	int ret = NODE_DESCR_SIZE + sizeof(keyLen) + htons(keyLen);
	if( ret % 2 != 0 ) ret++ ;

	return ret;
}

int main(int argc, char const *argv[])
{
	unsigned char secteur[1024];
	int blockSize=0;

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
    fillNodeDescriptor(&btnDesc, secteur, 1);

    struct BTHeaderRec bthRec ;
    fillBTHeaderRec(&bthRec, &secteur[NODE_DESCR_SIZE], 1);

  printf("\n------------- RootNode -------------------\n");
  	uint startRootNode = getNodeOffsetInCat(bthRec.rootNode, bthRec.nodeSize, startBTree);
  	printf("Real root node offset = %u\n", startRootNode);

  	unsigned char catBlock[bthRec.nodeSize];

  	readHDD(hdd, catBlock, startRootNode, bthRec.nodeSize);

  	struct BTNodeDescriptor rootNode;
  	fillNodeDescriptor(&rootNode, catBlock, 1);

  	uint *recOffsets = parseRecordOffsets(rootNode.numRecords, catBlock, bthRec.nodeSize);

  	struct BTreePointerRecord *records = getPointerRecs(rootNode.numRecords, recOffsets, catBlock);

	printf("\n------------- FirstLeafNode ----------------\n");

    uint startFirstLeafNode = getNodeOffsetInCat(bthRec.firstLeafNode, bthRec.nodeSize, startBTree);
    printf("startFirstLeafNode= %u\n", startFirstLeafNode);
    readHDD(hdd, secteur, startFirstLeafNode, 512);

    struct BTNodeDescriptor btnDescrFln;
    fillNodeDescriptor(&btnDescrFln, secteur, 1);

    struct HFSPlusCatalogKey catKeyFln;
    memcpy(&catKeyFln, &secteur[NODE_DESCR_SIZE], sizeof(struct HFSPlusCatalogKey));

    printf("keyLength:%u\n", htons(catKeyFln.keyLength));
    printf("parentID:%u\n", htonl(catKeyFln.parentID));
    printf("nodename length :%u\n", htons(catKeyFln.nodeName.length));
    // printf("nodename unicode :%s\n", catKeyFln.nodeName.unicode);

   	printf("-------------------------\n");

    struct HFSPlusCatalogFolder fold;
    fillCatFolderRec(&fold, &secteur[getFolderRecStartPos(catKeyFln.keyLength)], 1);
    
    printf("\n------------- LastLeafNode ----------------\n");
    uint startLastLeafNode = getNodeOffsetInCat( bthRec.lastLeafNode, bthRec.nodeSize, startBTree);
    printf("startLastLeafNode= %u\n", startLastLeafNode);
    readHDD(hdd, secteur, startLastLeafNode, 512);

    struct BTNodeDescriptor btnDescrLln;
    fillNodeDescriptor(&btnDescrLln, secteur, 1);


    memcpy(&catKeyFln, &secteur[NODE_DESCR_SIZE], sizeof(struct HFSPlusCatalogKey));

    printf("keyLength:%u\n", htons(catKeyFln.keyLength));
    printf("parentID:%u\n", htonl(catKeyFln.parentID));
    printf("nodename length :%u\n", htons(catKeyFln.nodeName.length));

    printf("\n------------- Read Applications Directory ----------------\n");
    HFSCatalogNodeID folderID = 1306;
    getLeafNode(folderID, startBTree);

    close(hdd);

	return 0;
}