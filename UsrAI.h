#ifndef USRAI_H
#define USRAI_H

#include "ai.h"
#include <unordered_map>

extern tagGame tagUsrGame;
extern ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

class UsrAI:public AI
{
public:
    UsrAI(){this->id=0;}
    ~UsrAI(){}

private:
    void processData() override;
    int AddToIns(instruction ins) override
        {
            UsrIns.lock.lock();
            ins.id=UsrIns.g_id;
            UsrIns.g_id++;
            UsrIns.instructions.push(ins);
            UsrIns.lock.unlock();
            return ins.id;
        }
    tagInfo getInfo(){return tagUsrGame.getInfo();}
    void clearInsRet() override
    {
        tagUsrGame.clearInsRet();
    }
    /*##########DO NOT MODIFY THE CODE IN THE CLASS##########*/
    double  calDistance(double dr1, double ur1, double dr2, double ur2);
       double blockToDetail(int block);
       void updateTerrainCache(tagInfo& info);
       bool findEmptyBlock(int& outDR, int& outUR, int size);
       void updateStage(tagInfo& info);
       void buildHuntWarehouse(tagInfo& info);
       void cutTree(tagInfo& info, int num, int resourceType,vector<int>& task,vector<bool>& Assigned);
       void hunting(tagInfo& info, int targetCount,vector<int>& task,vector<bool>& Assigned);
       void buildBuilding(tagInfo& info, int buildingType, int num,vector<bool>& Assigned);
       void armymanage(tagInfo& info);
       void priestManage(tagInfo& info);
       void priestFindway(tagInfo& info, int priestSN, double priestDR, double priestUR);
       void arrowTower(tagInfo& info);

};

#endif
