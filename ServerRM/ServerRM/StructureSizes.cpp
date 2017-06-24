
#include "ServerInclude.h"

typedef struct tagSCM_ThreadArgs {

    RM_COMMAND	cmd;
    SCMARGS		SCMArgs;
    CCMDATA		CCMDataIn;

}SCM_THREADARGS;

int ComputeStructSizes() {
    int CPMData;
    int SPMData;
    int PData;
    int KLRTTree, KLFileTransfer, SGFileData;
    int iCCMDataSize;
    int iSCMDataSize;
    int SCMthreadargs;

    int SizeofVoid = sizeof(void*);
    int iFindData = sizeof(WIN32_FIND_DATA);

    int iFBData = sizeof(FBDATA);

    iCCMDataSize = sizeof(CCMDATA);
    CPMData = sizeof(CPMDATA);

    iSCMDataSize = sizeof(SCMDATA);
    SCMthreadargs = sizeof(SCM_THREADARGS);
    SPMData = sizeof(SPMDATA);

    PData = sizeof(PDATA);

    KLRTTree = sizeof(KLRTTREE);
    KLFileTransfer = sizeof(FILE_TRANSFER_DATA);
    SGFileData = sizeof(SGFILEDATA);



    return 0;
}
