//
// Created by uidq3640 on 2024/11/25.
//

#ifndef NDKLEARNAPPLICATION_GYSOTOOLS_H
#define NDKLEARNAPPLICATION_GYSOTOOLS_H
#define LOGDD(format, ...) av_log(NULL, AV_LOG_DEBUG, "[%s] " format "\n", "GysoTools", ##__VA_ARGS__)


class GysoTools {
public:
    int count = 1;

    int sum(int n);
};

#endif //NDKLEARNAPPLICATION_GYSOTOOLS_H
