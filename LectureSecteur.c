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
#define TAILLE_SECTEUR 512 // Ne pas mettre en constante

typedef u_int32_t HFSCatalogNodeID;

struct BTreePointerRecord {
	u_int16_t keyLength;
	u_int32_t parentID;
	u_int16_t length;
  	unsigned char *key;
  	u_int32_t pointer;
};

int hdd=0;
char disk[100] = "/dev/rdisk2s1";

//refaire ca en mieux!!!
void printUTFasChar(uint length, unsigned char* key)
{
	printf("Key: ");
	for (int i = 0; i < length; i++)
	{
		if (i % 2 == 1)
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
	//printf("startpos: %u, size: %d\n", startpos, size );
	lseek( descr, startpos, SEEK_SET );
	ssize_t bytes = read( descr, secteur, size );
	if(bytes < 0)
		printf("Errorno read: %d  %s\n", errno, strerror(errno));
	//affichageSecteur(secteur, size);
}

void writeHDD(int descr, char* disk, uint startpos, unsigned char* buffer, int size)
{
	char continuer[11] = "poursuivre";
	char stopper[5] = "stop";
	char reponse[100];

	printf("*** ATTENTION *** Vous etes sur le point de modifier le disque: %s\n", disk);
	printf("Si vous voulez poursuivre ,ecrivez '%s':\n", continuer);
	printf("Sinon ecrivez 'stop'\n>> " );
	scanf("%s", reponse);
	
	while(strcmp(continuer, reponse) != 0 && strcmp(stopper, reponse) != 0)
	{
		printf("je n'ai pas compris votre saisie, veuillez réessayer\n");
		scanf("%s", reponse);
	}

	if(strcmp(continuer, reponse) == 0)
	{
		printf("on continue\n");

		unsigned char secteur[TAILLE_SECTEUR];
		memset(secteur, 65, sizeof(secteur));
		readHDD(descr, secteur, startpos, TAILLE_SECTEUR);
		memcpy(secteur, buffer, size);

		lseek(descr, startpos, SEEK_SET);
		ssize_t bytes = write(hdd, secteur, TAILLE_SECTEUR);
		if (bytes < 0 )
			printf("Errorno write: %d  %s\n", errno, strerror(errno));
	}
	else printf("ok on s'arrete la...\n");
	
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
		pointRecs[i].parentID = readULong(seek, nodeBlock);
		printf("ParentID: %u\n", pointRecs[i].parentID);
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

void readKeyCatalog(struct BTreePointerRecord* pRec, int* seek, unsigned char* sect)
{
	pRec->keyLength = readUShort(*seek, sect);
	*seek += sizeof(pRec->keyLength);

	pRec->parentID = readULong(*seek, sect);
	//printf("parentID: %u\n", pRec->parentID);

	*seek += sizeof(pRec->parentID);
	pRec->length = 2 * readUShort(*seek, sect); // on multiplie par 2 car length est en unicode16
	//printf("length: %u\n", pRec->length);

	*seek += sizeof(pRec->length);
	pRec->key = malloc(pRec->length);

	memcpy(pRec->key, &sect[*seek], pRec->length);
	//printUTFasChar(pRec->length, pRec->key);
	*seek += pRec->length;
}

uint readDataRecord(unsigned char* sect, uint offset, HFSCatalogNodeID cnid)
{
	int seek = offset;
	struct BTreePointerRecord pointRec;
	struct HFSPlusCatalogFolder foldInfo;
	struct HFSPlusCatalogFile fileInfo;
	struct HFSPlusCatalogThread threadInfo;

	readKeyCatalog(&pointRec, &seek, sect);

	printUTFasChar(pointRec.length, pointRec.key );
	if(cnid == pointRec.parentID)
	{
		printf("dossier/ fichier appartient au dossier recherché, offset:%u\n", offset);
	}
	

	// !! On ne met pas à jour seek car il s'agit simplement d'un test pour le switch
	u_int16_t type = readUShort(seek, sect);

	switch(type)
	{
		case kHFSPlusFolderRecord : 
			//printf("valence %d\n", readUShort(seek+4, sect));	
			//printf("%04x %04x\n", sect[seek], sect[seek+1] );
			memcpy(&foldInfo, &sect[seek], sizeof(struct HFSPlusCatalogFolder));
			// printf("nombre d'enfant : %u\n", htonl(foldInfo.valence));
			// printf(" record Type: %d\n", htons(foldInfo.recordType));
			// printf("---------------\n");
			break;
		case kHFSPlusFileRecord : 	
			memcpy(&fileInfo, &sect[seek], sizeof(struct HFSPlusCatalogFile));
			printf("ID fichier : %u\n", htonl(fileInfo.fileID));
			break;
		case kHFSPlusFolderThreadRecord: 	
			memcpy(&threadInfo, &sect[seek], sizeof(struct HFSPlusCatalogThread));

			break;
		case kHFSPlusFileThreadRecord: 	
			memcpy(&threadInfo, &sect[seek], sizeof(struct HFSPlusCatalogThread));
			break;
		default:
			printf("Aucune structure reconnue...\n");
			break;
	}
	return 0;
}

void readLeafNode(unsigned char* secteur, uint numRecords, uint* recOffsets, HFSCatalogNodeID cnid )
{
	for(int i = 0; i < numRecords; i++)
	{
		readDataRecord(secteur, recOffsets[i], cnid);
	}
}

uint readIndexRecord(unsigned char* sect, uint offset, uint* nodeNum)
{
	int seek = offset;

	struct BTreePointerRecord pointRec;

	readKeyCatalog(&pointRec, &seek, sect);

	pointRec.pointer = readULong(seek, sect);
	//printf("pointRec.pointer: %u\n", pointRec.pointer);
	*nodeNum = pointRec.pointer;

	//printf("parentID: %u\n",  pointRec.parentID);
	return pointRec.parentID;

}

void readIndexNode(unsigned char* secteur, uint numRecords, uint* recOffsets, uint* node, HFSCatalogNodeID cnid )
{
	// on alloue la memoire pour nos records du node
	uint nodeNum = 0;
	uint parentID = 0;

	for(int i = 0; i < numRecords; i++)
	{
		parentID = readIndexRecord(secteur, recOffsets[i], &nodeNum);
		if(parentID <= cnid)
		{
			*node = nodeNum;
			//printf("nodeNum: %u\n", nodeNum);
		}
		else 
		{
			printf("record offset: %u\n", recOffsets[i]);
			break;
		}
	}
	printf("new node to search : %u\n", *node);
}

void readNode(HFSCatalogNodeID cnid, uint *node, u_int16_t nodesize, uint offset, u_int8_t *height)
{
	
	unsigned char secteur[nodesize];
	readHDD(hdd, secteur, offset, nodesize);

	struct BTNodeDescriptor desc;

	fillNodeDescriptor(&desc, secteur, 0);

	*height = desc.height;

	uint *recOffsets = parseRecordOffsets(desc.numRecords, secteur, nodesize);

	switch(desc.kind)
	{
		case kBTIndexNode:
			readIndexNode(secteur, desc.numRecords, recOffsets, node, cnid);
			break;
		case kBTLeafNode: 
			readLeafNode(secteur, desc.numRecords, recOffsets, cnid);
			break;
		case kBTMapNode:
			break;
		case kBTHeaderNode:
			break;
		default:
			break;
	}

	free(recOffsets);

	return;
}

void getLeafNode(HFSCatalogNodeID cnid, uint startBTree)
{
	struct BTHeaderRec header;
	readHeaderRecord(&header, startBTree);

	u_int8_t height = header.treeDepth;
	uint node  = header.rootNode;
	u_int16_t nodesize = header.nodeSize;

	//printf("height:%u, node:%u, nodesize:%u\n", header.treeDepth, header.rootNode, header.nodeSize);
	while(height > 1)
	{
		printf("--------------------------------------------\n");
		printf("height =%d\n", height);
		printf("--------------------------------------------\n");
		uint offset = getNodeOffsetInCat(node, nodesize, startBTree);
		readNode(cnid, &node, nodesize, offset, &height);
		//height = 1;
	}
	printf("-----------------------------------\n");
	printf("offset:%u\n", getNodeOffsetInCat(node, nodesize, startBTree) );
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

	if(argc > 1)
	{
		strcpy(disk, argv[1]);
	}
	printf("----------------------------\n");
	printf("disque traité : %s\n", disk);
	printf("----------------------------\n\n");


	unsigned char secteur[1024];
	int blockSize=0;

	hdd = open(disk, O_RDWR);
	if ( hdd < 0 )
	{
		printf("Errorno open: %d  %s\n", errno, strerror(errno));
		return hdd;
	}

	unsigned char buffer[5]= {10,20,70,45,15};
	writeHDD(hdd, disk, 0, buffer, sizeof(buffer));

	
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

    printf("\n------------- Read Directory Content ----------------\n");
    HFSCatalogNodeID folderID = 2;
    getLeafNode(folderID, startBTree);


    close(hdd);

	return 0;
}