#ifndef OPERATION_MODEL_H
#define OPERATION_MODEL_H

struct OperationModel {
    char m_strFlag[8];
    char m_strIp[16];
    char m_cmd[8];
    char m_slot[16];
    int m_reqId;
};

#endif // OPERATION_MODEL_H 