#include<stdint.h>
#include<stdlib.h>
#include<stdio.h>
#include<memory.h>
#include"msd2smf.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
	printf("Need file path\n");
	return -1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if(NULL == fp){
	printf("open error\n");
	return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    uint8_t *src = malloc(size);
    if(NULL == src){
	printf("malloc error\n");
	fclose(fp);
	return -1;
    }
    fseek(fp, 0, SEEK_SET);
    fread(src, 1, size, fp);
    fclose(fp);

    size_t outSize = size*2;
    uint8_t* outBuff = (uint8_t*)malloc(outSize);
    int result = convert_msd_to_smf(src, size, outBuff, &outSize, 0);
    if (result != 0) {
	printf("convert error\n");
	return -1;
    }

    FILE *wfp = fopen("converted.mid", "wb");
    if(NULL == wfp){
	printf("open write file error\n");
	return -1;
    }
    fwrite(outBuff, outSize, 1, wfp);
    fclose(wfp);

    return 0;
}
