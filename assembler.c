#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define SIZE 50
struct opnode
{
    char mnemonic[10];
    int format;
    int opcode;
    struct opnode *next;
};
struct symnode
{
    char symbol[10];
    int address;
};
struct instruction
{
    char symbol[10];
    int opcode;
    char operand[10];
    char mnemonic[10];
    int location;
    int length; //指令長度
    int ni;
    int xbpe;
};
struct objcode
{
    char code[20];
    int location;
    int flag; //1-> mnemonic==RESW,RESB
};
struct mrecord
{
    int location;
    int num;
};
struct fr
{
    struct instruction ins;
    int type; // 4->direct   3->relative
    int obj_i;
};

struct instruction init_ins()
{
    struct instruction ins;
    strcpy(ins.symbol, "");
    ins.opcode = -1;
    strcpy(ins.operand, "");
    strcpy(ins.mnemonic, "");
    ins.location = -1;
    ins.length = 0;
    ins.ni = 0x3;   //SIC/XE
    ins.xbpe = 0x2; //PC
    return ins;
}

/*    全域變數     */
struct opnode OPTAB[SIZE];
struct symnode SYMTAB[SIZE];
int sym_num = 0;
struct mrecord mRecord[SIZE];
int m_i = 0;
struct objcode objArray[SIZE];
int obj_i = 0;
struct fr frRecord[SIZE]; //存foreword reference的指令
int fr_i = 0;

int LOCCTR; //location counter
int START;  //process start location
int line = 1;

char base_name[10] = "BASE";
int base;
char prog_name[10];
int prog_size;
int tcount = 0;
int error = 0; //有錯->1
int isEnd=0;
void init_OPTAB()
{
    int i;
    for (i = 0; i < SIZE; i++)
    {
        strcpy(OPTAB[i].mnemonic, "");
        OPTAB[i].format = 0;
        OPTAB[i].opcode = 0;
        OPTAB[i].next = NULL;
    }
}

//雜湊函數
int convert(char *m)
{
    int index = 0;
    int i;
    for (i = 0; i < strlen(m); i++)
    {
        index = index + (int)m[i];
    }
    index = index % SIZE;
    return index;
}

void put_OPTAB(char *m, int f, int o)
{
    int i = convert(m);
    //沒被放過
    if (strcmp(OPTAB[i].mnemonic, "") == 0)
    {
        //放入
        strcpy(OPTAB[i].mnemonic, m);
        OPTAB[i].format = f;
        OPTAB[i].opcode = o;
        OPTAB[i].next = NULL;
    }
    else
    {
        struct opnode *p = &OPTAB[i];
        while (p->next != NULL)
        {
            p = p->next;
        }
        struct opnode *tmp = (struct opnode *)malloc(sizeof(struct opnode));
        strcpy(tmp->mnemonic, m);
        tmp->format = f;
        tmp->opcode = o;
        tmp->next = p->next;
        p->next = tmp;
    }
}
// opcode=0 -> assembler directive
// opcode=-1 -> not found
int find_OPTAB(char *m, struct instruction *ins)
{
    if (strcmp(m, "START") == 0)
    {
        return 0;
    }
    else if (strcmp("END", m) == 0)
    {
        return 0;
    }
    else if (strcmp("WORD", m) == 0)
    {
        return 0;
    }
    else if (strcmp("BYTE", m) == 0)
    {
        return 0;
    }
    else if (strcmp("RESB", m) == 0)
    {
        return 0;
    }
    else if (strcmp("RESW", m) == 0)
    {
        return 0;
    }
    else if (strcmp("BASE", m) == 0)
    {
        return 0;
    }
    else
    {
        int i = convert(m);
        if (strcmp(OPTAB[i].mnemonic, "") == 0)
        {
            return -1;
        }
        else if (strcmp(OPTAB[i].mnemonic, m) == 0)
        {
            //找到
            if (ins->length == 0) //不是direct
            {
                ins->length = OPTAB[i].format;
            }
            return OPTAB[i].opcode;
        }
        else
        {
            //往後找
            struct opnode *p = &OPTAB[i];
            p = p->next;
            while (p != NULL)
            {
                //找到
                if (strcmp(p->mnemonic, m) == 0)
                {
                    if (ins->length == 0)
                    {
                        ins->length = p->format;
                    }
                    return p->opcode;
                }
                else
                {
                    p = p->next;
                }
            }
            //沒找到
            return -1;
        }
    }
}

void put_SYMTAB(char *symbol, int address)
{
    int i;
    for (i = 0; i < SIZE; i++)
    {
        if (strcmp(SYMTAB[i].symbol, symbol) == 0)
        {
            if (SYMTAB[i].address == -1)
            {
                //forword reference
                SYMTAB[i].address = address;
                return;
            }
            else
            {
                printf("line:%d  label [%s] 重複定義\n", line, symbol);
                error = 1;
                return;
            }
        }
    }
    //沒有，放進SYMTAB
    strcpy(SYMTAB[sym_num].symbol, symbol);
    SYMTAB[sym_num].address = address;
    sym_num++;
}

int find_SYMTAB(char *symbol)
{
    int i;
    for (i = 0; i < SIZE; i++)
    {
        //找到
        if (strcmp(SYMTAB[i].symbol, symbol) == 0)
        {
            return SYMTAB[i].address;
        }
    }
    //沒找到
    return -1;
}

void init_SYMTAB()
{
    int i;
    for (i = 0; i < SIZE; i++)
    {
        strcpy(SYMTAB[i].symbol, "");
        SYMTAB[i].address = -1;
    }
}
int isDecimalNum(char * str){
    int i;
    int size=strlen(str);
    char tmp[size];
    for(i=0;i<size;i++){
        if(!isdigit(str[i])){
            return 0;
        }
        tmp[i]=str[i];
    }
    tmp[size]='\0';
    return atoi(tmp);
}

//刪除空白、TAB
char *strim(char *str)
{
    char tmp[30];
    char *p = str;
    char *p2 = tmp;
    while (*p)
    {
        if (*p == '\t' || *p == ' ' || *p == '\n')
        {
        }
        else
        {
            *p2 = *p;
            p2++;
        }
        p++;
    }
    *p2 = '\0';
    strcpy(str, tmp);
    return str;
}

//把字串拆成指令
struct instruction split(char *str)
{
    char data[30];
    struct instruction ins = init_ins();
    //刪除註解
    char *pch = strchr(str, '.');
    if (pch == NULL)
    {
        //沒有註解
        strcpy(data, str);
    }
    else
    {
        //只存取註解前的資料
        int num = strlen(str) - strlen(pch);
        memset(data, '\0', sizeof(data));
        strncpy(data, str, num);
    }
    //mode
    pch = strchr(data, ',');
    if (pch != NULL)
    {
        char *pch2;
        int num = strlen(data) - strlen(pch);
        //index
        if (pch2 = strchr(pch, 'X'))
        {
            ins.xbpe = ins.xbpe + 0x8;
            char tmp[30];
            memset(tmp, '\0', sizeof(tmp));
            strncpy(tmp, data, num);
            strcpy(data, tmp);
        }
        //multiregister
        else
        {
            ins.length = 2;
        }
    }
    pch = strchr(data, '+');
    if (pch != NULL)
    {
        int num = strlen(data) - strlen(pch);
        data[num] = ' ';
        ins.xbpe = ins.xbpe - 0x1;
        ins.length = 4;
    }
    pch = strchr(data, '#');
    if (pch != NULL)
    {
        ins.ni = ins.ni - 0x2;
        int num = strlen(data) - strlen(pch);
        data[num] = ' ';
    }
    pch = strchr(data, '@');
    if (pch != NULL)
    {
        ins.ni = ins.ni - 0x1;
        int num = strlen(data) - strlen(pch);
        data[num] = ' ';
    }
    //切三段
    pch = strtok(data, " ");
    int i = 0;
    while (pch != NULL)
    {
        if(i>2){
            printf("line:%d  指令輸入錯誤\n", line);
            error=1;
            break;
        }
        strim(pch);
        if (i == 0)
        {
            int opcode = find_OPTAB(pch, &ins);
            if (opcode != -1)
            {
                // mnemonic
                strcpy(ins.mnemonic, pch);
                ins.opcode = opcode;
                i++;
            }
            else
            {
                //label
                strcpy(ins.symbol, pch);
            }
        }
        else
        {
            int opcode = find_OPTAB(pch, &ins);
            if (opcode != -1)
            {
                // mnemonic
                strcpy(ins.mnemonic, pch);
                ins.opcode = opcode;
            }
            else
            {
                //operand
                strcpy(ins.operand, pch);
            }
        }
        pch = strtok(NULL, " ");
        i++;
    }
    //錯誤的mnemonic
    if (strlen(ins.symbol) != 0 && strlen(ins.mnemonic) == 0)
    {
        printf("line:%d  mnemonic [%s] 輸入錯誤\n", line, ins.symbol);
        error = 1;
    }
    // LD... ST... WD 後面要有operand  
    pch = strstr(ins.mnemonic, "WD");
    if (pch != NULL){
        if(strcmp(ins.operand,"")==0){
            printf("line:%d  WD後面要有operand\n", line);
            error=1;
        }
    }
    pch = strstr(ins.mnemonic, "RD");
    if (pch != NULL){
        if(strcmp(ins.operand,"")==0){
            printf("line:%d  RD後面要有operand\n", line);
            error=1;
        }
    }
    pch = strstr(ins.mnemonic, "LD");
    if (pch != NULL){
        if(strcmp(ins.operand,"")==0){
            printf("line:%d  LOAD後面要有operand\n", line);
            error=1;
        }
    }
    pch = strstr(ins.mnemonic, "ST");
    if (pch != NULL){
        if(strcmp(ins.operand,"")==0 && strcmp(ins.mnemonic,"START")!=0){
            printf("line:%d  STORE後面要有operand\n", line);
            error=1;
        }
    }
    return ins;
}

void objcode_by_direct(struct instruction *comm, int i)
{
    // i==obj_i -> 翻成objcode ; i!=obj_i -> foreword reference處理
    int addr = find_SYMTAB(comm->operand);
    if (addr != -1)
    {
        //找到
        comm->opcode += comm->ni;
        sprintf(objArray[i].code, "%02x%x%05x", comm->opcode, comm->xbpe, addr);
        objArray[i].location = comm->location;
        if (i == obj_i)
        {
            obj_i++;
        }
    }
    else
    {
        if (i == obj_i)
        {
            put_SYMTAB(comm->operand, -1);
            //forword reference
            frRecord[fr_i].ins = *comm;
            frRecord[fr_i].type = 1;
            frRecord[fr_i].obj_i = i;
            fr_i++;
            strcpy(objArray[i].code, "00000000");
            objArray[i].location = comm->location;
            obj_i++;
        }
    }
}
void objcode_by_relative(struct instruction *ins, int i)
{

    //operand 是 label
    int ta = find_SYMTAB(ins->operand);
    if (ta != -1)
    {
        //找到
        ins->opcode += ins->ni;
        int pc = ins->location + 3;
        int disp = ta - pc;
        if (disp > 2047 || disp < -2048)
        {
            //base
            ins->xbpe = ins->xbpe + 0x2;
            disp = ta - base;
        }
        char buffer[10];
        char buffer2[10];
        sprintf(buffer, "%x", disp);
        if (strlen(buffer) == 8)
        {
            //取末3位
            strncpy(buffer2, buffer + 5, 3);
            buffer2[3] = '\0';
            sprintf(objArray[i].code, "%02x%x%s", ins->opcode, ins->xbpe, buffer2);
            objArray[i].location = ins->location;
        }
        else
        {
            sprintf(objArray[i].code, "%02x%x%03x", ins->opcode, ins->xbpe, disp);
            objArray[i].location = ins->location;
        }

        if (i == obj_i)
        {
            obj_i++;
        }
    }
    else
    {
        if (i == obj_i)
        {
            put_SYMTAB(ins->operand, -1);
            //forword reference
            frRecord[fr_i].ins = *ins;
            frRecord[fr_i].type = 2;
            frRecord[fr_i].obj_i = i;
            fr_i++;
            strcpy(objArray[obj_i].code, "000000");
            objArray[obj_i].location = ins->location;
            obj_i++;
        }
    }
}

void interpret(char *str)
{
    struct instruction comm = split(str);
    comm.location = LOCCTR;
    if (strcmp(comm.symbol, "") != 0)
    {
        put_SYMTAB(comm.symbol, LOCCTR);
    }
    //base位置
    if (strcmp(base_name, comm.symbol) == 0)
    {
        ////////
        base = comm.location;
    }
    //不用寫objcode
    if (strcmp(comm.mnemonic, "START") == 0)
    {
        if(strcmp(comm.operand, "") == 0){
            LOCCTR=0;
            comm.location = LOCCTR;
            START = LOCCTR;
        }else{
            sscanf(comm.operand, "%x", &LOCCTR);
            comm.location = LOCCTR;
            START = LOCCTR;
        }
        strcpy(prog_name, comm.symbol);
        if (strlen(prog_name) > 6)
        {
            printf("line:%d  程式名稱大於6個字 \n", line);
            error = 1;
            return;
        }
    }
    else if (strcmp(comm.mnemonic, "END") == 0)
    {
        int ta = find_SYMTAB(comm.operand);
        if(ta==-1){
            printf("line:%d  結束的operand錯誤 \n", line);
            error=1;
        }else{
            prog_size = LOCCTR - START + comm.length;
            isEnd=1;
        }
    }
    else if (strcmp(comm.mnemonic, "BASE") == 0)
    {
        //紀錄個base的位置
        strcpy(base_name, comm.operand);
    }
    else if (strcmp("RESB", comm.mnemonic) == 0)
    {
        if (strcmp(comm.operand, "0") == 0)
        {
            printf("line:%d  RESB的operand不為0 \n", line);
            error = 1;
            return;
        }
        if (isDecimalNum(comm.operand) == 0)
        {
            printf("line:%d  operand 輸入錯誤 \n", line);
            error = 1;
            return;
        }
        comm.length = isDecimalNum(comm.operand);
        strcpy(objArray[obj_i].code, "");
        objArray[obj_i].location = comm.location;
        objArray[obj_i].flag = 1;
        obj_i++;
    }
    else if (strcmp("RESW", comm.mnemonic) == 0)
    {
        if (strcmp(comm.operand, "0") == 0)
        {
            printf("line:%d  RESW的operand不為0 \n", line);
            error = 1;
            return;
        }
        if (isDecimalNum(comm.operand) == 0)
        {
            printf("line:%d  operand 輸入錯誤 \n", line);
            error = 1;
            return;
        }
        else
        {
            comm.length = isDecimalNum(comm.operand) * 3;
            strcpy(objArray[obj_i].code, "");
            objArray[obj_i].location = comm.location;
            objArray[obj_i].flag = 1;
            obj_i++;
        }
    }
    else if (strcmp("WORD", comm.mnemonic) == 0)
    {
        if (isDecimalNum(comm.operand) == 0 && strcmp(comm.operand, "0") != 0){
            printf("line:%d  operand 輸入錯誤 \n", line);
            error = 1;
            return;
        }
        comm.length = 3;
        int num = isDecimalNum(comm.operand);
        sprintf(objArray[obj_i].code, "%06x", num);
        objArray[obj_i].location = comm.location;
        obj_i++;
    }
    else if (strcmp("BYTE", comm.mnemonic) == 0)
    {

        if (comm.operand[0] == 'X')
        {
            int size = strlen(comm.operand) - 3;
            if (size % 2 == 0)
            {
                char *p = comm.operand;
                char tmp[10];
                strncpy(tmp, p + 2, size);
                tmp[size] = '\0';
                strcpy(objArray[obj_i].code, tmp);
                objArray[obj_i].location = comm.location;
                obj_i++;
                comm.length = size / 2;
            }
            else
            {
                //error
                printf("line:%d  X' '需要偶數個值\n", line);
                error = 1;
            }
        }
        else if (comm.operand[0] == 'C')
        {
            int size = strlen(comm.operand);
            int i;
            char tmp[10];
            char tmp2[10] = "";
            for (i = 0; i < size; i++)
            {
                if (i != 0 && i != 1 && i != size - 1)
                {
                    sprintf(tmp, "%x", comm.operand[i]);
                    strcat(tmp2, tmp);
                }
            }
            strcpy(objArray[obj_i].code, tmp2);
            objArray[obj_i].location = comm.location;
            obj_i++;
            comm.length = strlen(tmp2) / 2;
        }
        else
        {
            //error
            printf("line:%d  格式錯誤\n", line);
            error = 1;
        }
    }
    else if (comm.length == 4)
    {
        //operand 是 0 或 空值
        if (strcmp(comm.operand, "0") == 0 || strcmp(comm.operand, "") == 0)
        {
            comm.xbpe = 0x0;
            comm.opcode += comm.ni;
            sprintf(objArray[obj_i].code, "%02x%x00000", comm.opcode, comm.xbpe);
            objArray[obj_i].location = comm.location;
            obj_i++;
        }
        else
        {
            int ta = isDecimalNum(comm.operand);
            //operand 是 常數
            if (ta != 0)
            {
                comm.opcode += comm.ni;
                sprintf(objArray[obj_i].code, "%02x%x%05x", comm.opcode, comm.xbpe, ta);
                objArray[obj_i].location = comm.location;
                obj_i++;
            }
            else
            {
                //operand 是 symbol
                mRecord[m_i].location = comm.location - START + 1;
                mRecord[m_i].num = 5;
                m_i++;
                objcode_by_direct(&comm, obj_i);
            }
        }
    }
    else if (comm.length == 3)
    {
        //operand 是 0 或 空值
        if (strcmp(comm.operand, "0") == 0||strcmp(comm.operand, "") == 0)
        {
            comm.xbpe = 0x0;
            comm.opcode += comm.ni;
            sprintf(objArray[obj_i].code, "%02x%x000", comm.opcode, comm.xbpe);
            objArray[obj_i].location = comm.location;
            obj_i++;
        }
        else
        {
            int ta = isDecimalNum(comm.operand);
            //operand 是 常數
            if (ta != 0)
            {
                comm.opcode += comm.ni;
                comm.xbpe = 0x0;
                sprintf(objArray[obj_i].code, "%02x%x%03x", comm.opcode, comm.xbpe, ta);
                objArray[obj_i].location = comm.location;
                obj_i++;
            }
            else
            {
                //operand 是 symbol
                objcode_by_relative(&comm, obj_i);
            }
        }
    }
    else if (comm.length == 2)
    {
        char *pch = strtok(comm.operand, ",");
        int addr[2] = {-1, -1};
        int i = 0;
        while (pch != NULL)
        {
            if(i>1){
                printf("line:%d  register 太多\n", line);
                error=1;
                return;
            }
            if (strcmp(pch, "A") == 0)
            {
                addr[i] = 0;
            }
            else if (strcmp(pch, "X") == 0)
            {
                addr[i] = 1;
            }
            else if (strcmp(pch, "L") == 0)
            {
                addr[i] = 2;
            }
            else if (strcmp(pch, "B") == 0)
            {
                addr[i] = 3;
            }
            else if (strcmp(pch, "S") == 0)
            {
                addr[i] = 4;
            }
            else if (strcmp(pch, "T") == 0)
            {
                addr[i] = 5;
            }
            else if (strcmp(pch, "F") == 0)
            {
                addr[i] = 6;
            }
            else if (strcmp(pch, "PC") == 0)
            {
                addr[i] = 8;
            }
            else if (strcmp(pch, "SW") == 0)
            {
                addr[i] = 9;
            }
            else
            {
                printf("line:%d  register 輸入錯誤\n", line);
                error = 1;
                break;
            }
            pch = strtok(NULL, ",");
            i++;
        }
        if (addr[1] == -1)
        {
            // one register
            sprintf(objArray[obj_i].code, "%02x%x0", comm.opcode, addr[0]);
            objArray[obj_i].location = comm.location;
            obj_i++;
        }
        else
        {
            //two register
            sprintf(objArray[obj_i].code, "%02x%x%x", comm.opcode, addr[0], addr[1]);
            objArray[obj_i].location = comm.location;
            obj_i++;
        }
    }
    else if (comm.length == 1)
    {
        sprintf(objArray[obj_i].code, "%02x", comm.opcode);
        objArray[obj_i].location = comm.location;
        obj_i++;
    }
    else if (comm.length == 0)
    {
        return;
    }
    LOCCTR = LOCCTR + comm.length;
}

int main(int argc, char *argv[])
{
    int i;
    /*     初始化     */
    init_OPTAB();
    init_SYMTAB();

    /*     建立OPTAB     */
    FILE *finOP;
    char mnemonic[10];
    int opcode;
    int format;
    finOP = fopen("opcode", "r");
    if (finOP == NULL)
    {
        printf("opcode檔案開啟失敗\n");
        exit(1);
    }
    while (fscanf(finOP, "%[^,],%x,%x%*c", mnemonic, &format, &opcode) != EOF)
    {
        put_OPTAB(mnemonic, format, opcode);
    }
    fclose(finOP);

    /*     讀範例檔     */
    FILE *finEX;
    finEX = fopen("example2", "r");
    if (finEX == NULL)
    {
        printf("範例檔開啟失敗\n");
        exit(1);
    }

    //一行一行讀範例
    char str[50];
    while (fgets(str, 100, finEX) != NULL)
    {
        //刪除fget的'\n'
        str[strlen(str) - 1] = '\0';
        interpret(str);
        line++;
    }
    fclose(finEX);

    //檢查是不是SYMTAB都被填入
    for (i = 0; i < sym_num; i++)
    {
        if (SYMTAB[i].address == -1)
        {
            printf("有未定義的symbol : %s\n", SYMTAB[i].symbol);
            error = 1;
        }
    }

    if(isEnd==0){
        printf("程式沒有END \n");
        error=1;
    }

    //沒有錯誤，寫入objprogram
    if (error == 0)
    {

        //處理foreword reference
        for (i = 0; i < fr_i; i++)
        {
            if (frRecord[i].type == 2)
            {
                objcode_by_relative(&frRecord[i].ins, frRecord[i].obj_i);
            }
            else if (frRecord[i].type == 1)
            {
                objcode_by_direct(&frRecord[i].ins, frRecord[i].obj_i);
            }
        }

        /*     輸出objprogram     */
        FILE *finW;
        finW = fopen("objProgram.txt", "wb");
        if (finW == NULL)
        {
            printf("objProgram開啟失敗\n");
            exit(1);
        }

        //H record
        char tmp[60];
        sprintf(tmp, "H%-6s%06x%06x\n", prog_name, START, prog_size);
        fprintf(finW, "%s", tmp);
        //T record
        char buffer[100];
        sprintf(tmp, "T%06x", objArray[0].location);
        fprintf(finW, "%s", tmp);
        for (i = 0; i < obj_i; i++)
        {
            int size = strlen(objArray[i].code) / 2;
            tcount += size;
            if (tcount >= 30)
            {
                int c = tcount - size;
                sprintf(tmp, "%02x%s\n", c, buffer);
                fprintf(finW, "%s", tmp);
                tcount = 0;
                strcpy(buffer, "");
            }
            if (objArray[i].flag == 1 && objArray[i + 1].flag != 1)
            {
                int c = tcount - size;
                sprintf(tmp, "%02x%s\n", c, buffer);
                fprintf(finW, "%s", tmp);
                tcount = 0;
                strcpy(buffer, "");
            }
            if (tcount == 0 && objArray[i].flag == 1 && i!=0)
            {
                tcount += size;
                sprintf(tmp, "T%06x", objArray[i+1].location);
                fprintf(finW, "%s", tmp);
            }else if (tcount == 0){
                tcount += size;
                sprintf(tmp, "T%06x", objArray[i].location);
                fprintf(finW, "%s", tmp);
            }
            //內容
            sprintf(tmp, "%s", objArray[i].code);
            strcat(buffer, tmp);
        }
        int size = strlen(objArray[i].code) / 2;
        int c = tcount - size;
        sprintf(tmp, "%02x%s", c, buffer);
        fprintf(finW, "%s", tmp);
        //M record
        for (i = 0; i < m_i; i++)
        {
            sprintf(tmp, "\nM00%04x%02x+%s", mRecord[i].location, mRecord[i].num, prog_name);
            fprintf(finW, "%s", tmp);
        }
        //E record
        sprintf(tmp, "\nE00%04x", START);
        fprintf(finW, "%s", tmp);
        fclose(finW);
    }
    else
    {
        printf("翻譯失敗\n");
    }
}
