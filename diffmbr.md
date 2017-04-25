DIFF MBR first file added "axszw"

--VOLUME HEADER--
1: modifyDate
2: fileCount
3: freeBlocks
4: nextAllocationBlock
5: nextCatalogID
6: writeCount
~Optional: lastMountedVersion <--- give unique ID to our program

--FILE ALLOCATION--
1: set bitmap at offset

--JOURNAL HEADER--
1: start
2: end
3: checksum

--CATALOG FILE BTREE HEADER RECORD--
1: leafRecords (total number of records in leaf nodes)

--FIRST LEAF NODE--
1: numRecords
2: add thread record for the file
3: add record offset at the end of node

--LAST LEAF NODE--
1: numRecords
2: contentModDate
3: attributeModDate
4: textEncoding???
5: add file record
6: offset to the file record at end of node
