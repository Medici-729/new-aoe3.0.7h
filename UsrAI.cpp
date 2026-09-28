#include "UsrAI.h"
#include <cmath>
#include <cstdlib>
#include <climits>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
using namespace std;

tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/
#define MAP_SIZE 100
#define stageExplore 1
#define stageDefense1 2
#define stageDefense2 3
#define stageAttack 4
static int stage=1;
static int builderSN=-1;
static int terrainCache[MAP_SIZE][MAP_SIZE];
#define FARMER_IDLE 0
#define FARMER_WALK 1
#define FARMER_BUILD 2
#define FARMER_WOOD 3
#define FARMER_BERRY 4
#define FARMER_HUNT 5
#define FARMER_MEAT 6
#define FARMER_FARM 7
#define FARMER_GOLD 8
static map<int, int> farmer_state;
static vector<int> taskBuild;

//距离计算函数
double  UsrAI::calDistance(double dr1,double ur1,double dr2,double ur2){
    double ddr=dr1-dr2;
    double dur=ur1-ur2;
    return sqrt(ddr*ddr+dur*dur);
}
//块坐标转换为细节坐标
double UsrAI::blockToDetail(int block) {
    return block * BLOCKSIDELENGTH + BLOCKSIDELENGTH / 2.0;
}
//地图缓存更新
void UsrAI:: updateTerrainCache(tagInfo& info) {
    for (int i=0;i<MAP_SIZE;i++){
        for(int j=0;j<MAP_SIZE;j++){ terrainCache[i][j]=-1;}// 初始化所有格子为 -1
    }
    if(info.theMap!=0){
        for(int i=0;i<MAP_SIZE;i++){
            for(int j=0;j<MAP_SIZE;j++){
        auto& t=(*info.theMap)[i][j];
        if(t.type==MAPPATTERN_GRASS&&t.height>=0){terrainCache[i][j]=0;}
            }
        }
    }// 标记空地(0)
    for(tagBuilding& b:info.buildings){
        int size = 2;
        switch (b.Type) {
            case BUILDING_HOME:      size = 2; break;
            case BUILDING_ARROWTOWER:size = 2; break;
            case BUILDING_FARM:      size = 3; break;
            case BUILDING_CENTER:    size = 3; break;
            case BUILDING_STOCK:     size = 3; break;
            case BUILDING_GRANARY:   size = 3; break;
            case BUILDING_ARMYCAMP:  size = 3; break;
            case BUILDING_RANGE:     size = 3; break;
            case BUILDING_STABLE:    size = 3; break;
            case BUILDING_MARKET:    size = 3; break;
            case BUILDING_COLLAGE:   size = 3; break;
            case BUILDING_SIEGE:     size = 3; break;
            default:                 size = 2; break;
            }
        for(int i=b.BlockDR;i<b.BlockDR+size&&i<MAP_SIZE;i++){
            for(int j=b.BlockUR;j<b.BlockUR+size&&j<MAP_SIZE;j++){
                terrainCache[i][j]=1;
            }
        }
    }//标记建筑(1)
    for(tagResource& r:info.resources){
        if(r.BlockDR>=0&&r.BlockDR<MAP_SIZE&&r.BlockUR>=0&&r.BlockUR<MAP_SIZE)
        {terrainCache[r.BlockDR][r.BlockUR]=2;}
        }//标记资源(2)
    for(tagFarmer& f:info.farmers){
        if(f.BlockDR>=0&&f.BlockDR<MAP_SIZE&&f.BlockUR>=0&&f.BlockUR<MAP_SIZE){
         terrainCache[f.BlockDR][f.BlockUR]=3;
        }
     }
    for(tagArmy& a:info.armies){
        if(a.BlockDR>=0&&a.BlockDR<MAP_SIZE&&a.BlockUR>=0&&a.BlockUR<MAP_SIZE){
         terrainCache[a.BlockDR][a.BlockUR]=3;
        }
     }//标记单位(3)
}
//找空地
static int centerDR = -1;
static int centerUR = -1;
bool UsrAI::findEmptyBlock(int& outDR, int& outUR, int size) {
    static int offset = 0;
       offset = (offset + 7) % 40;
       int startI = centerDR + offset - 20;
       int startJ = centerUR + offset - 20;
       for (int i = max(0, startI); i <= min(MAP_SIZE - size, centerDR + 30); i++) {
           for (int j = max(0, startJ); j <= min(MAP_SIZE - size, centerUR + 30); j++) {
               bool ok = true;
               for (int di = 0; di < size && ok; di++) {
                   for (int dj = 0; dj < size && ok; dj++) {
                       if (terrainCache[i+di][j+dj] != 0) ok = false;
                   }
               }
               if (ok) {
                   outDR = i;
                   outUR = j;
                   return true;
               }
           }
       }
       for (int i = 0; i <= MAP_SIZE - size; i++) {
           for (int j = 0; j <= MAP_SIZE - size; j++) {
               bool ok = true;
               for (int di = 0; di < size && ok; di++) {
                   for (int dj = 0; dj < size && ok; dj++) {
                       if (terrainCache[i+di][j+dj] != 0) ok = false;
                   }
               }
               if (ok) {
                   outDR = i;
                   outUR = j;
                   return true;
               }
           }
       }
       return false;
}
//阶段切换
void UsrAI::updateStage(tagInfo& info) {
    int frame=info.GameFrame;
    if(frame<6000) {
        stage=stageExplore;
    } else if(frame<13500) {
        stage=stageDefense1;
    } else if(frame<21000) {
        stage=stageDefense2;
    } else {
        stage=stageAttack;
    }
}
void UsrAI::buildHuntWarehouse(tagInfo& info){
        int gazelleDR = -1, gazelleUR = -1;
        for (tagResource& r : info.resources) {
            if (r.Type != RESOURCE_GAZELLE) continue;
            if (r.Blood <= 0 && r.Cnt <= 0) continue;
            gazelleDR = r.BlockDR;
            gazelleUR = r.BlockUR;
            break;
        }

        if (gazelleDR == -1) return;
        int bestDR = -1, bestUR = -1;
            double bestDist = 1e9;

            for (int i = max(0, gazelleDR - 10); i <= min(MAP_SIZE - 3, gazelleDR + 10); i++) {
                for (int j = max(0, gazelleUR - 10); j <= min(MAP_SIZE - 3, gazelleUR + 10); j++) {
                    bool ok = true;
                    for (int di = 0; di < 3 && ok; di++) {
                        for (int dj = 0; dj < 3 && ok; dj++) {
                            if (terrainCache[i+di][j+dj] != 0) ok = false;
                        }
                    }
                    if (!ok) continue;

                    double d = calDistance(blockToDetail(i), blockToDetail(j),
                                           blockToDetail(gazelleDR), blockToDetail(gazelleUR));
                    if (d < bestDist) {
                        bestDist = d;
                        bestDR = i;
                        bestUR = j;
                    }
                }
            }

            if (bestDR == -1) return;   // 找不到空地，不建
            if (builderSN == -1) return;   // 没有建设者，不建

            HumanBuild(builderSN, BUILDING_STOCK, bestDR, bestUR);
}
void UsrAI::assignNewFarmers(tagInfo& info) {
    // ============================================================
    // 1. 统计当前各状态人数
    // ============================================================
    int berryCount = 0, huntCount = 0, woodCount = 0, buildCount = 0;
    for (auto& p : farmer_state) {
        if (p.second == FARMER_BERRY) berryCount++;
        if (p.second == FARMER_HUNT) huntCount++;
        if (p.second == FARMER_WOOD) woodCount++;
        if (p.second == FARMER_BUILD) buildCount++;
    }
    
    // ============================================================
    // 2. 计算目标人数（用你的比例）
    // ============================================================
    int farmercount = 0;
    for (tagFarmer& f : info.farmers) {
        if (f.FarmerSort == FARMERTYPE_FARMER && f.Blood > 0) farmercount++;
    }
    int extra = max(farmercount - 8, 0);
    
    int berryTarget, huntTarget, woodTarget, buildTarget;
    static bool hasUpgraded = false;
    
    if (!hasUpgraded) {
        berryTarget = 3 + min(extra * 2 / 10, 3);
        huntTarget = 2 + extra * 4 / 10;
        woodTarget = 2 + extra * 2 / 10;
        buildTarget = 1 + extra * 2 / 10;
    } else {
        berryTarget = 2 + extra * 4 / 10;
        huntTarget = 2 + extra * 2 / 10;
        woodTarget = 1 + extra * 2 / 10;
        buildTarget = 1;
    }
    
    // ============================================================
    // 3. 分配新村民
    // ============================================================
    for (tagFarmer& f : info.farmers) {
        if (f.FarmerSort != FARMERTYPE_FARMER) continue;
        if (f.Blood <= 0) continue;
        if (farmer_state.find(f.SN) != farmer_state.end()) continue;
        
        // 建设者单独处理
        if (f.SN == builderSN) {
            farmer_state[f.SN] = FARMER_BUILD;
            buildCount++;
            continue;
        }
        
        // 按优先级分配
        if (berryCount < berryTarget) {
            farmer_state[f.SN] = FARMER_BERRY;
            berryCount++;
        } else if (huntCount < huntTarget) {
            farmer_state[f.SN] = FARMER_HUNT;
            huntCount++;
        } else if (woodCount < woodTarget) {
            farmer_state[f.SN] = FARMER_WOOD;
            woodCount++;
        } else if (buildCount < buildTarget) {
            farmer_state[f.SN] = FARMER_BUILD;
            buildCount++;
        } else {
            farmer_state[f.SN] = FARMER_BERRY;
            berryCount++;
        }
    }
}
int UsrAI::findUnassignedResource(tagInfo& info, int resourceType) {
    // 收集已被分配的资源 SN
    vector<int> assignedSN;
    for (tagFarmer& f : info.farmers) {
        if (f.WorkObjectSN != 0) {
            assignedSN.push_back(f.WorkObjectSN);
        }
    }
    
    // 找无人采集的最近资源
    int bestSN = -1;
    double bestDist = 1e9;
    for (tagResource& r : info.resources) {
        if (r.Type != resourceType) continue;
        if (r.Blood <= 0 && r.Cnt <= 0) continue;
        
        bool taken = false;
        for (int sn : assignedSN) {
            if (sn == r.SN) { taken = true; break; }
        }
        if (taken) continue;
        
        double d = calDistance(0, 0, r.DR, r.UR);
        if (d < bestDist) { bestDist = d; bestSN = r.SN; }
    }
    return bestSN;
}
void UsrAI::manageFarmers(tagInfo& info) {
    // 1. 分配新村民
    assignNewFarmers(info);
    
    // 2. 找空闲村民，分配具体目标
    for (tagFarmer& f : info.farmers) {
        if (f.FarmerSort != FARMERTYPE_FARMER) continue;
        if (f.Blood <= 0) continue;
        if (f.NowState != HUMAN_STATE_IDLE) continue;
        if (f.SN == builderSN) continue;   // 建设者交给 buildBuilding
        
        int state = farmer_state[f.SN];
        
        switch (state) {
            case FARMER_BERRY: {
                int targetSN = findUnassignedResource(info, RESOURCE_BUSH);
                if (targetSN != -1) {
                    HumanAction(f.SN, targetSN);
                } else {
                    farmer_state[f.SN] = FARMER_HUNT;   // 改打猎
                }
                break;
            }
            case FARMER_HUNT: {
                int targetSN = findUnassignedResource(info, RESOURCE_GAZELLE);
                if (targetSN != -1) {
                    HumanAction(f.SN, targetSN);
                } else {
                    farmer_state[f.SN] = FARMER_WOOD;   // 改砍树
                }
                break;
            }
            case FARMER_WOOD: {
                int targetSN = findUnassignedResource(info, RESOURCE_TREE);
                if (targetSN != -1) {
                    HumanAction(f.SN, targetSN);
                }
                break;
            }
            case FARMER_GOLD: {
                int targetSN = findUnassignedResource(info, RESOURCE_GOLD);
                if (targetSN != -1) {
                    HumanAction(f.SN, targetSN);
                }
                break;
            }
        }
        
        return;   // 每帧只分配一个
    }
}

//建筑：市镇中心，谷仓，市场，农田，兵营、靶场 、马厩
void UsrAI:: buildBuilding(tagInfo& info,int buildingType,int num){
   if (builderSN == -1) return;
    bool builderIdle = false;
    for (tagFarmer& f : info.farmers) {
        if (f.SN == builderSN && f.Blood > 0 && 
            f.NowState == HUMAN_STATE_IDLE) {
            builderIdle = true;
            break;
        }
    }
    if (!builderIdle) return;
    int size = 3;
    if (buildingType == BUILDING_HOME || buildingType == BUILDING_ARROWTOWER) size = 2;
    int buildDR, buildUR;
    if (!findEmptyBlock(buildDR, buildUR, size)) return;
    HumanBuild(builderSN, buildingType, buildDR, buildUR);
}
//军队管理
void UsrAI::armymanage(tagInfo& info){
    for(tagArmy& a:info.armies){
        if(a.Sort==AT_PRIEST) continue;
        if(a.Blood<=0) continue;
        if(a.NowState!=HUMAN_STATE_IDLE&&a.NowState!=HUMAN_STATE_WALKING) continue;
        int targetSN=-1;
        double minDist=1e9;
        for(tagArmy& enemy:info.enemy_armies){
            double d=calDistance(a.DR,a.UR,enemy.DR,enemy.UR);
            if (d > 15 * BLOCKSIDELENGTH) continue;
            if (enemy.Sort == AT_CHARIOT_ARCHER || enemy.Sort == AT_COMPOSITE_BOWMAN || enemy.Sort == AT_STONE_THROWER){
                 targetSN=enemy.SN;
                 break;
           }
        }
        if(targetSN==-1&&!info.enemy_armies.empty()){
            for(tagArmy& enemy:info.enemy_armies){
                double d = calDistance(a.DR, a.UR, enemy.DR, enemy.UR);
                if (d < 15 * BLOCKSIDELENGTH && d < minDist) {
                    minDist = d;
                    targetSN = enemy.SN;
                 }
            }
        }
        if(targetSN==-1&&stage>=stageAttack&&!info.enemy_buildings.empty()){
            for(tagBuilding& enemy : info.enemy_buildings){
                double eDR = blockToDetail(enemy.BlockDR);
                double eUR = blockToDetail(enemy.BlockUR);
                double d = calDistance(a.DR, a.UR, eDR, eUR);
                if (d < 20 * BLOCKSIDELENGTH && d < minDist) {
                    minDist = d;
                    targetSN = enemy.SN;
                 }
            }
        }
        if(targetSN!=-1){
            HumanAction(a.SN,targetSN);
        }
    }
}
//祭司管理：转化敌人，躲避
void UsrAI::priestManage(tagInfo& info) {
    // 1. 找祭司
    int priestSN = -1;
    double priestDR = 0, priestUR = 0;
    int convertCooldown = 0;
    for (tagArmy& a : info.armies) {
        if (a.Sort == AT_PRIEST && a.Blood > 0) {
            priestSN = a.SN;
            priestDR = a.DR;
            priestUR = a.UR;
            convertCooldown = a.ConvertCooldown;
            break;
        }
    }
    if (priestSN == -1) return;

    // 2. 检查敌人（提前到20格）
    bool enemyDetected = false;
    double enemyDR = 0, enemyUR = 0;
    int enemySN = -1;
    double minEnemyDist = 1e9;
    for (tagArmy& e : info.enemy_armies) {
        double d = calDistance(priestDR, priestUR, e.DR, e.UR);
        if (d < 20 * BLOCKSIDELENGTH && d < minEnemyDist) {
            minEnemyDist = d;
            enemyDetected = true;
            enemyDR = e.DR;
            enemyUR = e.UR;
            enemySN = e.SN;
        }
    }

    // 3. 有敌人 → 躲箭塔 + 转换
    if (enemyDetected) {
        // 找最近箭塔
        double towerDR = -1, towerUR = -1;
        double minTowerDist = 1e9;
        for (tagBuilding& b : info.buildings) {
            if (b.Type == BUILDING_ARROWTOWER && b.Percent == 100) {
                double bDR = blockToDetail(b.BlockDR);
                double bUR = blockToDetail(b.BlockUR);
                double d = calDistance(priestDR, priestUR, bDR, bUR);
                if (d < minTowerDist) {
                    minTowerDist = d;
                    towerDR = bDR;
                    towerUR = bUR;
                }
            }
        }

        if (towerDR != -1) {
            // 祭司往箭塔跑
            if (minTowerDist > 2 * BLOCKSIDELENGTH) {
                // 根据敌人方向，站在箭塔反方向
                double dirDR = enemyDR - towerDR;
                double dirUR = enemyUR - towerUR;
                double len = sqrt(dirDR*dirDR + dirUR*dirUR);
                if (len > 0.1) {
                    double standDR = towerDR - (dirDR / len) * 1.5 * BLOCKSIDELENGTH;
                    double standUR = towerUR - (dirUR / len) * 1.5 * BLOCKSIDELENGTH;
                    HumanMove(priestSN, standDR, standUR);
                } else {
                    HumanMove(priestSN, towerDR, towerUR + 1.5 * BLOCKSIDELENGTH);
                }
            } else {
                // 已经在箭塔旁边，检查敌人是否进射程
                double enemyToTower = calDistance(enemyDR, enemyUR, towerDR, towerUR);
                if (enemyToTower < 7 * BLOCKSIDELENGTH && convertCooldown == 0 && enemySN != -1) {
                    bool convertingstate=false;
                    for(tagArmy& a:info.armies){
                        if(a.SN==priestSN&&a.WorkObjectSN==enemySN){
                            convertingstate=true;
                            break;
                        }
                 if(!convertingstate) HumanAction(priestSN, enemySN);
                }
            }
          }
        }
        return;
    }
    if (stage != stageExplore) {
            double towerDR = -1, towerUR = -1;
            double minTowerDist = 1e9;
            for (tagBuilding& b : info.buildings) {
                if (b.Type == BUILDING_ARROWTOWER && b.Percent > 0) {
                    double bDR = blockToDetail(b.BlockDR);
                    double bUR = blockToDetail(b.BlockUR);
                    double d = calDistance(priestDR, priestUR, bDR, bUR);
                    if (d < minTowerDist) {
                        minTowerDist = d;
                        towerDR = bDR;
                        towerUR = bUR;
                    }
                }
            }

            if (towerDR != -1 && minTowerDist > 2 * BLOCKSIDELENGTH) {
                HumanMove(priestSN, towerDR, towerUR + 1.5 * BLOCKSIDELENGTH);
            }
            return;
        }
    // 4. 没有敌人 → 探路
    if(stage==stageExplore)
    priestFindway(info, priestSN, priestDR, priestUR);
}
//祭司探路
void UsrAI::priestFindway(tagInfo& info, int priestSN, double priestDR, double priestUR) {
        static int step = 0;
        static int lastFrame = 0;
        static double lastFrameDR = -1, lastFrameUR = -1;
        static int stuckFrames = 0;

        // 初始化
        if (lastFrameDR < 0) {
            lastFrameDR = priestDR;
            lastFrameUR = priestUR;
            lastFrame = info.GameFrame;
            step = rand() % 4;
        }
        if (info.GameFrame - lastFrame >= 25) {
            double moved = calDistance(lastFrameDR, lastFrameUR, priestDR, priestUR);
            if (moved < 1.0 * BLOCKSIDELENGTH) {
                stuckFrames++;
            } else {
                stuckFrames = 0;
            }
            lastFrameDR = priestDR;
            lastFrameUR = priestUR;
            lastFrame = info.GameFrame;
        }
        if (stuckFrames > 0) {
            int perp1 = (step + 1) % 4;
            int perp2 = (step + 3) % 4;
            step = (rand() % 2 == 0) ? perp1 : perp2;

            stuckFrames = 0;
            lastFrameDR = priestDR;
            lastFrameUR = priestUR;
            lastFrame = info.GameFrame;
        }
        int curBlockDR = (int)(priestDR / BLOCKSIDELENGTH);
        int curBlockUR = (int)(priestUR / BLOCKSIDELENGTH);
        int dist = 10;
        int tBDR = curBlockDR, tBUR = curBlockUR;
        switch (step) {
            case 0: tBDR = curBlockDR - dist; break;   // 左
            case 1: tBUR = curBlockUR - dist; break;   // 上
            case 2: tBDR = curBlockDR + dist; break;   // 右
            case 3: tBUR = curBlockUR + dist; break;   // 下
        }
        if (tBDR < 0 || tBDR >= MAP_SIZE || tBUR < 0 || tBUR >= MAP_SIZE) {
            step = (step + 1 + rand() % 3) % 4;
            return;
        }
        if (terrainCache[tBDR][tBUR] != 0) {
            int perp1 = (step + 1) % 4;
            int perp2 = (step + 3) % 4;
            step = (rand() % 2 == 0) ? perp1 : perp2;
            return;
        }
        HumanMove(priestSN, blockToDetail(tBDR), blockToDetail(tBUR));
}
void UsrAI::arrowTower(tagInfo& info){
    for (tagBuilding& b : info.buildings) {
        if (b.Type != BUILDING_ARROWTOWER) continue;
        if (b.Percent < 100) continue;
        int targetSN = -1;
        int bestDis2 = 1000000000;
        int towerRange = 7;
        int towerRange2 = towerRange * towerRange;
        for (tagArmy& e : info.enemy_armies) {
            int d2 = (b.BlockDR - e.BlockDR) * (b.BlockDR - e.BlockDR) +
                     (b.BlockUR - e.BlockUR) * (b.BlockUR - e.BlockUR);
            if (d2 <= towerRange2 && d2 < bestDis2) {
                bestDis2 = d2;
                targetSN = e.SN;
            }
        }
        bool currentTargetInRange = false;
        if (b.Project != -1) {
            for (tagArmy& e : info.enemy_armies) {
                if (e.SN == b.Project) {
                    int d2 = (b.BlockDR - e.BlockDR) * (b.BlockDR - e.BlockDR) +
                             (b.BlockUR - e.BlockUR) * (b.BlockUR - e.BlockUR);
                    if (d2 <= towerRange2) currentTargetInRange = true;
                    break;
                }
            }
         }
        if (currentTargetInRange) continue;
        if (targetSN != -1) {
            HumanAction(b.SN, targetSN);
        }
    }
}

/* ============================== 主入口 ============================== */
void UsrAI::processData ()
{tagInfo info = getInfo();
    if (info.GameFrame % 5 != 0) return;
    for (tagBuilding& b : info.buildings) {
        if (b.Type == BUILDING_CENTER && b.Percent > 0) {
            centerDR = b.BlockDR;
            centerUR = b.BlockUR;
            break;
        }
    }
    if(builderSN==-1){
        for(tagFarmer& f:info.farmers){
            if(f.FarmerSort==FARMERTYPE_FARMER&&f.Blood>0){
                builderSN=f.SN;
                break;
            }
        }
    }
    bool builderAlive = false;
    for (tagFarmer& f : info.farmers) {
        if (f.SN == builderSN && f.Blood > 0) {
            builderAlive = true;
            break;
        }
    }
    if (!builderAlive) {
        builderSN = -1;
        for (tagFarmer& f : info.farmers) {
            if (f.FarmerSort == FARMERTYPE_FARMER && f.Blood > 0) {
                builderSN = f.SN;
                break;
            }
        }
    }
    updateTerrainCache(info);
    updateStage(info);
    priestManage(info);
    arrowTower(info);
    static bool hasUpgraded=false;
    int farmercount=0;
    for(tagFarmer& f:info.farmers){
       if(f.FarmerSort==FARMERTYPE_FARMER&&f.Blood>0) farmercount++;
    }
    manageFarmers(info);
    if (info.Human_MaxNum < 16 && info.Wood >= 30 && !hasUpgraded) {
            buildBuilding(info, BUILDING_HOME,1);
        }
        if (hasUpgraded && info.Human_MaxNum < 48 && info.Wood >= 30) {
            buildBuilding(info, BUILDING_HOME, 1);
        }
    static bool hasHuntware=false;
    if(!hasHuntware&&info.Wood>=120){
        buildHuntWarehouse(info);
        hasHuntware=true;
    }
    static bool homeenough=false;
    if(info.Human_MaxNum>=16) homeenough=true;
    bool hasMarket=false;
    bool hasArmyCamp=false;
    bool hasRange=false;
    bool hasStable=false;
    bool hasCollage=false;
    int centerSN=-1;
    for (tagBuilding& b : info.buildings) {
       if (b.Percent <=0) continue;
       switch (b.Type) {
           case BUILDING_CENTER: centerSN = b.SN; break;
           case BUILDING_MARKET: hasMarket = true; break;
           case BUILDING_ARMYCAMP: hasArmyCamp = true; break;
           case BUILDING_RANGE: hasRange = true; break;
           case BUILDING_STABLE: hasStable = true; break;
           case BUILDING_COLLAGE: hasCollage = true; break;
       }
   }
   //市镇中心功能实现
    bool hasOrederUpgrade=false;
   for(tagBuilding& b:info.buildings){
       if(b.SN==centerSN&&b.Project==0){
           if(info.Meat>=50&&info.Human_Num<=info.Human_MaxNum&&farmercount<=24){
               BuildingAction(centerSN,BUILDING_CENTER_CREATEFARMER);
               break;
           }
       }
   }
   if (info.civilizationStage == CIVILIZATION_TOOLAGE && info.Meat >= 800&&!hasUpgraded) {
       if (hasMarket && (hasRange || hasStable)) {
           for (tagBuilding& b : info.buildings) {
               if (b.SN == centerSN && b.Project == 0) {
                   BuildingAction(centerSN, BUILDING_CENTER_UPGRADE);
                   hasOrederUpgrade=true;
                   if(info.civilizationStage==CIVILIZATION_BRONZEAGE){
                       hasUpgraded = true;
                   }
                   break;
               }
           }
       }
   }
   //建筑安排
   if(homeenough==true){
       if (!hasMarket&&info.Wood >= 150) {
           buildBuilding(info, BUILDING_MARKET, 1);
       }
       if (!hasArmyCamp&&info.Wood >= 125) {
           buildBuilding(info, BUILDING_ARMYCAMP, 1);
       }
       if (!hasRange &&stage >= stageDefense1 && info.Wood >= 150&&hasArmyCamp) {
           buildBuilding(info, BUILDING_RANGE, 1);
       }
       if (!hasStable&&stage >= stageDefense1 && info.Wood >= 150&&hasArmyCamp) {
           buildBuilding(info, BUILDING_STABLE, 1);
       }
       if (!hasCollage&&stage >= stageDefense2 && info.Wood >= 180&&info.civilizationStage == CIVILIZATION_BRONZEAGE) {
           buildBuilding(info, BUILDING_COLLAGE, 1);
       }
       if (info.Human_Num >= info.Human_MaxNum - 2 && info.Wood >= 30&&hasUpgraded) {
           buildBuilding(info, BUILDING_HOME, 1);
       }
   }
   //仓库研发攻防
   static bool hasTool=false;
   static bool hasDefense=false;
   for(tagBuilding& b:info.buildings){
       if(b.Type==BUILDING_STOCK&&b.Project==0&&hasOrederUpgrade){
           if(!hasTool&&info.Meat>=100){
             BuildingAction(b.SN, BUILDING_STOCK_UPGRADE_USETOOL);
             hasTool=true;
             continue;
           }
           if(info.civilizationStage>=CIVILIZATION_BRONZEAGE&&info.Meat>=75){
              BuildingAction(b.SN, BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY);
              hasDefense = true;
              continue;
           }
       }
   }

   //市场研发科技
   static bool hasWheel=false;
   static bool hasWoodUp=false;
   static bool hasFarmUp=false;
   static bool hasGoldUp=false;
   static bool hasStoneUp = false;
   for(tagBuilding& b:info.buildings){
       if(b.Type==BUILDING_MARKET&&b.Project==0){
           if(!hasWheel&&info.Meat>=150&&info.Wood>=100&&hasOrederUpgrade){
               BuildingAction(b.SN, BUILDING_MARKET_WHEEL_UPGRADE);
               hasWheel = true;
               continue;
           }
           if(!hasWoodUp&&info.Meat>=120&&info.Wood>=75&&hasOrederUpgrade){
              BuildingAction(b.SN, BUILDING_MARKET_WOOD_UPGRADE);
              hasWoodUp = true;
              continue;
           }
           if(!hasFarmUp&&hasOrederUpgrade&&info.Meat>=150&&info.Wood>=50){
               BuildingAction(b.SN, BUILDING_MARKET_FARM_UPGRADE);
               hasFarmUp = true;
               continue;
           }
           if(!hasGoldUp&&info.Meat>=120&&info.Wood>=100&&hasOrederUpgrade){
               BuildingAction(b.SN, BUILDING_MARKET_GOLD_UPGRADE);
               hasGoldUp = true;
               continue;
           }
           if (!hasStoneUp && info.Meat >= 100 && info.Stone >= 50 && hasOrederUpgrade) {
                BuildingAction(b.SN, BUILDING_MARKET_STONE_UPGRADE);
                hasStoneUp = true;
                continue;
           }
       }
   }

   //兵营训练士兵
   static bool hasUpgradedClubman = false;
   if(hasUpgraded){
       for(tagBuilding& b:info.buildings){
           if(b.Type==BUILDING_ARMYCAMP&&b.Project==0){
               if(info.civilizationStage>=CIVILIZATION_BRONZEAGE&&info.Meat>=35&&info.Gold>=15&&info.Human_Num<info.Human_MaxNum){
                  BuildingAction(b.SN, BUILDING_ARMYCAMP_CREATE_BROADSWORD);
                  continue;
               }
               if(info.Meat>=50&&info.Human_Num<info.Human_MaxNum){
                  BuildingAction(b.SN, BUILDING_ARMYCAMP_CREATE_CLUBMAN);
                  continue;
               }
               if(!hasUpgradedClubman&&info.civilizationStage>=CIVILIZATION_TOOLAGE&&info.Meat>=100){
                  BuildingAction(b.SN, BUILDING_ARMYCAMP_UPGRADE_CLUBMAN);
                  hasUpgradedClubman=true;
                  continue;
               }
           }
       }
   }
   //靶场训练弓箭手
   for(tagBuilding& b:info.buildings){
       if(b.Type==BUILDING_RANGE&&b.Project==0){
           if(hasUpgraded&&info.Human_Num<info.Human_MaxNum&&info.Wood>=20&&info.Meat>=40){
               BuildingAction(b.SN, BUILDING_RANGE_CREATE_BOWMAN);
               continue;
           }
       }
   }
   //马厩训练骑兵
   if(stage>=stageDefense2){
       for(tagBuilding& b:info.buildings){
           if(b.Type==BUILDING_STABLE&&b.Project==0&&hasUpgraded){
               if(info.civilizationStage>=CIVILIZATION_BRONZEAGE&&info.Human_Num<info.Human_MaxNum&&info.Meat>=70&&info.Gold>=80){
                   BuildingAction(b.SN, BUILDING_STABLE_CREATE_CAVALRY);
                   continue;
               }
               if(info.Human_Num<=info.Human_MaxNum&&info.Meat>=60){
                   BuildingAction(b.SN, BUILDING_STABLE_CREATE_SCOUT);
                   continue;
               }
           }
       }
   }
   if(stage>=stageDefense2){
       for(tagBuilding& b:info.buildings){
           if(b.Type==BUILDING_COLLAGE&&b.Project==0){
               if(info.Human_Num<info.Human_MaxNum&&info.Meat>=60&&info.Gold>=40){
                   BuildingAction(b.SN, BUILDING_COLLAGE_CREATE_HOPLITE);
                   continue;
               }
           }
       }
   }
   //谷仓研发箭塔
   static bool hasArrowTower=false;
   if(!hasArrowTower&&info.Meat>=50&&hasOrederUpgrade){
       for(tagBuilding& b:info.buildings){
           if(b.Type==BUILDING_GRANARY&&b.Project==0){
               BuildingAction(b.SN,BUILDING_GRANARY_ARROWTOWER);
               hasArrowTower=true;
               break;
           }
       }
   }
   armymanage(info);

}
