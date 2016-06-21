#include "MarketSpi.h"
#include <signal.h>

using namespace std;

CThostFtdcMdApi * mApi;
string pidPath;

void shutdown(int sig)
{
    mApi->Release();
    remove(pidPath.c_str());
    cout << "MarketSrv stop success!" << endl;
}

int main(int argc, char const *argv[])
{
    // 初始化参数
    parseIniFile("../etc/config.ini");
    string flowPath = getOptionToString("flow_path_m");
    string logPath  = getOptionToString("log_path");
    string bid      = getOptionToString("market_broker_id");
    string userID   = getOptionToString("market_user_id");
    string password = getOptionToString("market_password");
    string mURL     = getOptionToString("market_front");

    string instrumnetIDs = getOptionToString("instrumnet_id");

    int kLineSrvID  = getOptionToInt("k_line_service_id");
    pidPath  = getOptionToString("pid_path");

    int isDev = getOptionToInt("is_dev");
    int db;
    if (isDev) {
        db = getOptionToInt("rds_db_dev");
    } else {
        db = getOptionToInt("rds_db_online");
    }

    std::vector<string> iIDs = Lib::split(instrumnetIDs, "/");
    std::map<string, std::vector<string> > stopTradeTimes;
    for (int i = 0; i < iIDs.size(); ++i)
    {
        string tmp = getOptionToString("stop_trade_time_" + iIDs[i]);
        stopTradeTimes[iIDs[i]] = Lib::split(tmp, "/");
    }

    int tradeLogicSrvID = getOptionToInt("trade_logic_service_id");

    signal(30, shutdown);
    ofstream pid;
    pid.open(pidPath.c_str(), ios::out);
    pid << getpid();
    pid.close();

    // 初始化交易接口
    mApi = CThostFtdcMdApi::CreateFtdcMdApi(flowPath.c_str());
    MarketSpi mSpi(mApi, logPath, kLineSrvID, bid, userID, password, instrumnetIDs, db, stopTradeTimes, tradeLogicSrvID); // 初始化回调实例
    mApi->RegisterSpi(&mSpi);
    mApi->RegisterFront(Lib::stoc(mURL));
    mApi->Init();
    cout << "MarketSrv start success!" << endl;
    mApi->Join();

    return 0;
}
