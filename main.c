#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char** argv){
    unsigned long chunkSize = 1;
    unsigned long skipSize = 0;
    int verbose_flag = 0;
    enum {
        END_ON_ANY,
        END_WITH_FIRST,
        END_ON_ALL,
    } endCondition = END_ON_ANY;
    int noFirstFileSkip = 0;
    FILE* fout = NULL;
    {
        const struct option long_options[] = {
        /*0*/{"verbose"     , no_argument, &verbose_flag, 1},
        /*1*/{"help"        , no_argument, NULL, 'h'},
        /*2*/{"chunk"       , required_argument, NULL, 'n'},
        /*3*/{"skip"        , required_argument, NULL, 0},
        /*4*/{"endOnAny"    , no_argument, (int*)&endCondition, END_ON_ANY},
        /*5*/{"endWithFirst", no_argument, (int*)&endCondition, END_WITH_FIRST},
        /*6*/{"endOnAll"    , no_argument, (int*)&endCondition, END_ON_ALL},
        /*7*/{"noSkipFile1" , no_argument, &noFirstFileSkip, 1},
        /*8*/{"output"      , required_argument, NULL, 'o'},
        /*9*/{0, 0, NULL, 0}
        };
        int long_option_i = 0;
        char res;
        while((res = getopt_long(argc, argv, "hn:vo:", long_options, &long_option_i)) != -1){
            switch (res){
            case 0:
                if(long_options[long_option_i].flag != NULL) break;
                switch(long_option_i){
                    case 3: //skip
                        skipSize = strtoul(optarg, NULL, 10);
                    break;
                    case 7: //dont skip file 1
                        noFirstFileSkip = strtol(optarg, NULL, 10);
                        if(noFirstFileSkip > 1)
                            exit(-1);
                    break;
                }
            break;
            case 'n':
                chunkSize = strtoul(optarg, NULL, 10);
            break;
            case 'v':
                verbose_flag = 1;
            break;
            case 'o':
                if(optarg[0] == '-' && optarg[1] == '\0'){
                    fout = stdout;
                } else {
                    fout = fopen(optarg, "w");
                }
            break;
            case '?':
            case 'h':
                fprintf(stderr, "Usage:\nbyteblender [OPTION]… [FILENAME]…\n"
                    "\t-h, --help\tshow this message and exit(0)\n"
                    "\t-v, --verbose\tenable verbose mode\n"
                    "\t-n, --chunk\tset size of blended chunks\n"
                    "\t-o, --output\toutput file, use minus sign '-' for stdout\n"
                    "\t--skip\tset header size - amount of bytes from the beginning of the file that will not be affected.\n"
                    "\t--noSkipFile1\tshould --skip option affect first file (default: 0)\n"
                    "\tEnd condition selection:\n"
                    "\t\t--endOnAny\texit when any of the input files ends\n"
                    "\t\t--endWithFirst\texit when the first input file ends\n"
                    "\t\t--endOnAll\texit when all of the input files end\n"
                );
                exit(0);
            }
            if(errno == ERANGE) exit(-1);
        }
    }

    if(fout == NULL){
        exit(-2);
    }

    const int flen = argc - optind;
    if(verbose_flag){
        fprintf(stderr, "Chunk size: %d\n", chunkSize);
        fprintf(stderr, "Files to blend: %d\n", flen);
        fprintf(stderr, "Header skip: %d bytes\n", skipSize);
        const char* endConditionStrings[] = {"on any file end","end with first file","on all files end"};
        fprintf(stderr, "End condition: %s\n", endConditionStrings[endCondition]);
        if(skipSize > 0 && noFirstFileSkip)
            fprintf(stderr, "First file (Filename: %s) was not skipped.\n", argv[optind]);
    }
    // Open all files for read
    FILE* f[flen];
    for(int i = optind; i < argc; i++){
        FILE* fi = fopen(argv[i], "r");
        if(fi == NULL) exit(-3);
        f[i-optind] = fi;
    }
    // Skip headers of all files (except the first one if set to)
    for(int i = noFirstFileSkip; i < flen; i++){
        fseek(f[i], skipSize, SEEK_SET);
    }

    char buffer[chunkSize];
    size_t read = 0, write;
    for(;;){
        read = 0;
        for(int i = 0; i < flen; i++){
            if(f[i] == NULL)
                continue;
            if(verbose_flag)
                fprintf(stderr, "Blending… Reading file %d     \r", i+1);
            for(read = 0; read < chunkSize && !feof(f[i]); read += fread(buffer+read, 1, chunkSize-read, f[i]));
            for(write = read; write > 0; write -= fwrite(buffer, sizeof(char), read, fout));
            if(feof(f[i])){
                if(verbose_flag)
                    fprintf(stderr, "Blending… End  of file %d\n", i+1);
                switch (endCondition)
                {
                    case END_ON_ANY:
                        exit(0);
                    break;
                    case END_WITH_FIRST:
                        if(i == 0)
                            exit(0);
                        // Intentional fallthrough
                    case END_ON_ALL:
                        fclose(f[i]);
                        f[i] = NULL;
                    break;
                }
            }
            read = 1;
        }
        if(read == 0){
            // All files have reached EOF
            exit(0);
        }
    }
}