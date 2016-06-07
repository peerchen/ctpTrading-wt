#include "TradeLogic.h"

#define OUU 0
#define OUD 1
#define ODU 2
#define ODD 3
#define CU 4
#define CD 5


TradeLogic::TradeLogic(int peroid, double thresholdTrend, double thresholdVibrate,
    int serviceID, string logPath, int db,
    string stopTradeTime, string instrumnetID, int kRange)
{
    _instrumnetID = instrumnetID;

    _peroid = peroid;
    _thresholdTrend = thresholdTrend;
    _thresholdVibrate = thresholdVibrate;

    _logPath = logPath;
    _forecastID = 0;
    _kRange = kRange;

    // 初始化模型参数
    _pUp2Up = _pUp2Down = _pDown2Up = _pDown2Down = 0;
    _countUp2Up = _countUp2Down = _countDown2Up = _countDown2Down = 0;

    // 初始化停止交易时间
    std::vector<string> times = Lib::split(stopTradeTime, "/");
    std::vector<string> hm;
    int i;
    for (i = 0; i < times.size(); ++i)
    {
        TRADE_HM tmp = {0};
        hm = Lib::split(times[i], ":");
        tmp.hour = Lib::stoi(hm[0]);
        tmp.min = Lib::stoi(hm[1]);
        _timeHM.push_back(tmp);
    }

    _store = new Redis("127.0.0.1", 6379, db);
    _tradeStrategySrvClient = new QClient(serviceID, sizeof(MSG_TO_TRADE_STRATEGY));

}

TradeLogic::~TradeLogic()
{
    // delete _store;
    // delete _tradeStrategySrvClient;
    cout << "~TradeLogicSrv" << endl;
}

void TradeLogic::init()
{
    string tickData = _store->get("MARKOV_HISTORY_KLINE_TICK_" + _instrumnetID);
    std::vector<string> ticks = Lib::split(tickData, "_");
    TickData tick = {0};
    for (int i = 0; i < ticks.size(); ++i)
    {
        tick.price = Lib::stod(ticks[i]);
        _tick(tick);
    }
}

void TradeLogic::_tick(TickData tick)
{
    // 如果相邻tick没变化，则忽略
    if (_tickGroup.size() > 0) {
        TickData last = _tickGroup.front();
        if (last.price == tick.price) return;
    }

    // 保存tick
    _tickGroup.push_front(tick);
    while (_tickGroup.size() >= 4) {
        _tickGroup.pop_back();
    }

    // tick足够三个，计算一组转换
    if (_tickGroup.size() < 3) return;
    TickData t[3];
    list<TickData>::iterator i;
    int j = 2;
    for (i = _tickGroup.begin(); i != _tickGroup.end(); i++) {
        t[j--] = *i;
    }

    if (t[0].price > t[1].price) {
        if (t[1].price > t[2].price) {
            _countDown2Down++;
            _transTypeList.push_front(TRANS_TYPE_DOWN2DOWN);
        } else {
            _countDown2Up++;
            _transTypeList.push_front(TRANS_TYPE_DOWN2UP);
        }
    } else {
        if (t[1].price > t[2].price) {
            _countUp2Down++;
            _transTypeList.push_front(TRANS_TYPE_UP2DOWN);
        } else {
            _countUp2Up++;
            _transTypeList.push_front(TRANS_TYPE_UP2UP);
        }
    }

    // 检查转换列表是否够用，够用删除相应的记录
    while (_transTypeList.size() > _peroid) {
        int type = _transTypeList.back();
        _transTypeList.pop_back();
        switch (type) {
            case TRANS_TYPE_DOWN2DOWN:
                _countDown2Down--;
                break;
            case TRANS_TYPE_UP2DOWN:
                _countUp2Down--;
                break;
            case TRANS_TYPE_DOWN2UP:
                _countDown2Up--;
                break;
            case TRANS_TYPE_UP2UP:
                _countUp2Up--;
                break;
            default:
                break;
        }
    }
}

void TradeLogic::_calculateUp(double u2d, double u2u)
{
    if (u2d + u2u > 0) {
        _pUp2Up = (double)u2u / ((double)u2u + (double)u2d);
        _pUp2Down = 1 - _pUp2Up;
    }
    //log
    ofstream info;
    Lib::initInfoLogHandle(_logPath, info);
    info << "TradeLogicSrv[calculateUp]";
    info << "|iID|" << _instrumnetID;
    info << "|pUp2Up|" << _pUp2Up;
    info << "|pUp2Down|" << _pUp2Down;
    info << "|UU_UD_DU_DD|" << u2u << "," << u2d << "," << _countDown2Up << "," << _countDown2Down;
    info << "|kIndex|" << _kIndex;
    info << endl;
    info.close();
}


void TradeLogic::_calculateDown(double d2u, double d2d)
{
    if (d2u + d2d > 0) {
        _pDown2Up = (double)d2u / ((double)d2u + (double)d2d);
        _pDown2Down = 1 - _pDown2Up;
    }
    //log
    ofstream info;
    Lib::initInfoLogHandle(_logPath, info);
    info << "TradeLogicSrv[calculateDown]";
    info << "|iID|" << _instrumnetID;
    info << "|pDown2Up|" << _pDown2Up;
    info << "|pDown2Down|" << _pDown2Down;
    info << "|UU_UD_DU_DD|" << _countUp2Up << "," << _countUp2Down << "," << d2u << "," << d2d;
    info << "|kIndex|" << _kIndex;
    info << endl;
    info.close();
}

bool TradeLogic::_isCurrentUp()
{
    list<TickData>::iterator it = _tickGroup.begin();
    TickData last = *it;
    it++;
    TickData before = *it;
    bool isUp = true;
    if (last.price < before.price) isUp = false;
    return isUp;
}

void TradeLogic::_setRollbackID(int type, int id)
{
    switch (type) {
        case OUU:
            _rollbackOpenUUID = id;
            _store->set("OUU_" + _instrumnetID, Lib::itos(id));
            break;
        case OUD:
            _rollbackOpenUDID = id;
            _store->set("OUD_" + _instrumnetID, Lib::itos(id));
            break;
        case ODU:
            _rollbackOpenDUID = id;
            _store->set("ODU_" + _instrumnetID, Lib::itos(id));
            break;
        case ODD:
            _rollbackOpenUUID = id;
            _store->set("ODD_" + _instrumnetID, Lib::itos(id));
            break;
        case CU:
            _rollbackCloseUID = id;
            _store->set("CU_" + _instrumnetID, Lib::itos(id));
            break;
        case CD:
            _rollbackCloseDID = id;
            _store->set("CD_" + _instrumnetID, Lib::itos(id));
            break;
        default:
            break;
    }
}

void TradeLogic::_rollback()
{
    if (_rollbackOpenUUID > 0) {
        _sendRollBack(_rollbackOpenUUID);
        // _rollbackOpenUUID = 0;
        _setRollbackID(OUU, 0);
    }
    if (_rollbackOpenUDID > 0) {
        _sendRollBack(_rollbackOpenUDID);
        // _rollbackOpenUDID = 0;
        _setRollbackID(OUD, 0);
    }
    if (_rollbackOpenDUID > 0) {
        _sendRollBack(_rollbackOpenDUID);
        // _rollbackOpenDUID = 0;
        _setRollbackID(ODU, 0);
    }
    if (_rollbackOpenDDID > 0) {
        _sendRollBack(_rollbackOpenDDID);
        // _rollbackOpenDDID = 0;
        _setRollbackID(ODD, 0);
    }

    if (_rollbackCloseUID > 0) {
        _sendRollBack(_rollbackCloseUID);
        // _rollbackCloseUID = 0;
        _setRollbackID(CU, 0);
    }
    if (_rollbackCloseDID > 0) {
        _sendRollBack(_rollbackCloseDID);
        // _rollbackCloseDID = 0;
        _setRollbackID(CD, 0);
    }
}

void TradeLogic::_forecastNothing(TickData tick)
{
    // 只发一单
    if (_isCurrentUp()) { // 当前是up

        _calculateDown(_countDown2Up, _countDown2Down);
        if (_pDown2Up > _thresholdVibrate) {
            _forecastID++;
            // _rollbackOpenUDID = _forecastID;
            _setRollbackID(OUD, _forecastID);
            _sendMsg(MSG_TRADE_BUYOPEN, tick.price - _kRange, true, _forecastID, true);
        }

        _calculateUp(_countUp2Down, _countUp2Up + 1);
        if (_pUp2Down > _thresholdVibrate) {
            _forecastID++;
            // _rollbackOpenUUID = _forecastID;
            _setRollbackID(OUU, _forecastID);
            _sendMsg(MSG_TRADE_SELLOPEN, tick.price + _kRange, true, _forecastID, true);
        }

    } else { // 当前是down

        _calculateDown(_countDown2Up, _countDown2Down + 1);
        if (_pDown2Up > _thresholdVibrate) {
            _forecastID++;
            // _rollbackOpenDDID = _forecastID;
            _setRollbackID(ODD, _forecastID);
            _sendMsg(MSG_TRADE_BUYOPEN, tick.price - _kRange, true, _forecastID, true);
        }

        _calculateUp(_countUp2Down, _countUp2Up);
        if (_pUp2Down > _thresholdVibrate) {
            _forecastID++;
            // _rollbackOpenDUID = _forecastID;
            _setRollbackID(ODU, _forecastID);
            _sendMsg(MSG_TRADE_SELLOPEN, tick.price + _kRange, true, _forecastID, true);
        }
    }
}

void TradeLogic::_forecastBuyOpened(TickData tick)
{
    bool isCloseMain = true;
    if (_isCurrentUp()) { // 当前是up

        _calculateUp(_countUp2Down, _countUp2Up + 1);
        if (_pUp2Up <= _thresholdTrend) {
            _forecastID++;
            // _rollbackCloseUID = _forecastID;
            _setRollbackID(CU, _forecastID);
            isCloseMain = true;
            // if (_pUp2Down > _threshold_open) {
            //     _forecastID++;
            //     _rollbackOpenUUID = _forecastID;
            //     isCloseMain = false;
            // }
            _sendMsg(MSG_TRADE_SELLCLOSE, tick.price + _kRange, true, _rollbackCloseUID, isCloseMain);
            if (!isCloseMain)
                _sendMsg(MSG_TRADE_SELLOPEN, tick.price + _kRange, true, _rollbackOpenUUID, true);
        }

    } else { // 当前是down

        _calculateUp(_countUp2Down, _countUp2Up);
        if (_pUp2Up <= _thresholdTrend) {
            _forecastID++;
            // _rollbackCloseDID = _forecastID;
            _setRollbackID(CD, _forecastID);
            isCloseMain = true;
            // if (_pUp2Down > _threshold_open) {
            //     _forecastID++;
            //     _rollbackOpenDUID = _forecastID;
            //     isCloseMain = false;
            // }
            _sendMsg(MSG_TRADE_SELLCLOSE, tick.price + _kRange, true, _rollbackCloseDID, isCloseMain);
            if (!isCloseMain)
                _sendMsg(MSG_TRADE_SELLOPEN, tick.price + _kRange, true, _rollbackOpenDUID, true);
        }

    }
}

void TradeLogic::_forecastSellOpened(TickData tick)
{
    bool isCloseMain = true;
    if (_isCurrentUp()) { // 当前是up

        _calculateDown(_countDown2Up, _countDown2Down);
        if (_pDown2Down <= _thresholdTrend) {
            _forecastID++;
            // _rollbackCloseUID = _forecastID;
            _setRollbackID(CU, _forecastID);
            isCloseMain = true;
            // if (_pDown2Up > _threshold_open) {
            //     _forecastID++;
            //     _rollbackOpenUDID = _forecastID;
            //     isCloseMain = false;
            // }
            _sendMsg(MSG_TRADE_BUYCLOSE, tick.price - _kRange, true, _rollbackCloseUID, isCloseMain);
            if (!isCloseMain)
                _sendMsg(MSG_TRADE_BUYOPEN, tick.price - _kRange, true, _rollbackOpenUDID, true);
        }

    } else { // 当前是down

        _calculateDown(_countDown2Up, _countDown2Down + 1);
        if (_pDown2Down <= _thresholdTrend) {
            _forecastID++;
            // _rollbackCloseDID = _forecastID;
            _setRollbackID(CD, _forecastID);
            isCloseMain = true;
            // if (_pDown2Up > _threshold_open) {
            //     _forecastID++;
            //     _rollbackOpenDDID = _forecastID;
            //     isCloseMain = false;
            // }
            _sendMsg(MSG_TRADE_BUYCLOSE, tick.price - _kRange, true, _rollbackCloseDID, isCloseMain);
            if (!isCloseMain)
                _sendMsg(MSG_TRADE_BUYOPEN, tick.price - _kRange, true, _rollbackOpenDDID, true);
        }
    }
}

void TradeLogic::onKLineOpen(KLineBlock block, TickData tick)
{
    _kIndex = block.getIndex();
    if (_transTypeList.size() < _peroid - 1) {
        return; // 计算概率条件不足，不做操作
    }
    usleep(100 * 1000);
    int status = _getStatus();

    ofstream info;
    Lib::initInfoLogHandle(_logPath, info);
    info << "TradeLogicSrv[forecastBegin]";
    info << "|status|" << status;
    info << "|kIndex|" << _kIndex;
    info << endl;
    info.close();

    // _rollbackOpenUUID = _rollbackOpenUDID = _rollbackOpenDDID = _rollbackOpenDUID = 0;
    // _rollbackCloseDID = _rollbackCloseUID = 0;
    _setRollbackID(OUU, 0);
    _setRollbackID(OUD, 0);
    _setRollbackID(ODD, 0);
    _setRollbackID(ODU, 0);
    _setRollbackID(CU, 0);
    _setRollbackID(CD, 0);
    switch (status) {
        case TRADE_STATUS_NOTHING:
            _forecastNothing(tick);
            break;
        case TRADE_STATUS_BUYOPENED:
            _forecastBuyOpened(tick);
            break;
        case TRADE_STATUS_SELLOPENED:
            _forecastSellOpened(tick);
            break;
        default:
            break;
    }
}

void TradeLogic::onKLineClose(KLineBlock block, TickData tick)
{
    _tick(tick);
    int status = _getStatus();
    //log
    ofstream info;
    Lib::initInfoLogHandle(_logPath, info);
    info << "TradeLogicSrv[onKLineClose]";
    info << "|status|" << status;
    info << endl;
    info.close();

    _rollback();
    switch (status) {

        case TRADE_STATUS_BUYCLOSING: // 预测单未成，回滚然后再下单，对于不是预测单导致的，忽略
        case TRADE_STATUS_SELLOPENED:

            if (_isCurrentUp()) {
                _calculateUp(_countUp2Down, _countUp2Up);
                if (_pUp2Down <= _thresholdVibrate) {
                    _sendMsg(MSG_TRADE_BUYCLOSE, tick.bidPrice1, false, 0, true);
                }
            } else {
                _calculateDown(_countDown2Up, _countDown2Down);
                if (_pDown2Down <= _thresholdTrend) {
                    _sendMsg(MSG_TRADE_BUYCLOSE, tick.bidPrice1, false, 0, true);
                }
            }
            break;

        case TRADE_STATUS_SELLCLOSING:
        case TRADE_STATUS_BUYOPENED:

            if (_isCurrentUp()) {
                _calculateUp(_countUp2Down, _countUp2Up);
                if (_pUp2Up <= _thresholdTrend ) { // 不满足买开，平仓
                    _sendMsg(MSG_TRADE_SELLCLOSE, tick.askPrice1, false, 0, true);
                }

            } else {
                _calculateDown(_countDown2Up, _countDown2Down);
                if (_pDown2Up <= _thresholdVibrate) { // 不满足买开，平
                    _sendMsg(MSG_TRADE_SELLCLOSE, tick.askPrice1, false, 0, true);
                }
            }
            break;

        case TRADE_STATUS_BUYOPENING:
        case TRADE_STATUS_SELLOPENING:
            // _rollback();
            break;
        case TRADE_STATUS_NOTHING:
            if (_isCurrentUp()) {
                _calculateUp(_countUp2Down, _countUp2Up);
                if (_pUp2Up > _thresholdTrend) { // 买开
                    _sendMsg(MSG_TRADE_BUYOPEN, tick.price + 10, false, 0, true);
                }
            } else {
                _calculateDown(_countDown2Up, _countDown2Down);
                if (_pDown2Down > _thresholdTrend) { // 卖开
                    _sendMsg(MSG_TRADE_SELLOPEN, tick.price - 10, false, 0, true);
                }
            }
            break;

        default:
            break;
    }

}

int TradeLogic::_getStatus()
{
    string status = _store->get("TRADE_STATUS_" + _instrumnetID);
    return Lib::stoi(status);
}

void TradeLogic::_sendMsg(int msgType, double price, bool isForecast, int forecastID, bool isMain)
{
    string now = Lib::getDate("%H:%M");
    std::vector<string> nowHM = Lib::split(now, ":");
    for (int i = 0; i < _timeHM.size(); ++i)
    {
        if (_timeHM[i].hour == Lib::stoi(nowHM[0]) && Lib::stoi(nowHM[1]) > _timeHM[i].min) {
            return;
        }
    }

    //log
    ofstream info;
    Lib::initInfoLogHandle(_logPath, info);
    info << "TradeLogicSrv[sendMsg]";
    info << "|iID|" << _instrumnetID;
    info << "|forecastID|" << forecastID;
    info << "|isForecast|" << isForecast;
    info << "|isMain|" << isMain;
    info << "|action|" << msgType;
    info << "|price|" << price;
    info << "|kIndex|" << _kIndex << endl;
    info.close();

    MSG_TO_TRADE_STRATEGY msg = {0};
    msg.msgType = msgType;
    msg.price = price;
    msg.kIndex = _kIndex;
    msg.total = 1;
    msg.isForecast = isForecast;
    if (isForecast) msg.forecastID = forecastID;
    msg.isMain = isMain;
    strcpy(msg.instrumnetID, Lib::stoc(_instrumnetID));
    _tradeStrategySrvClient->send((void *)&msg);

}

void TradeLogic::_sendRollBack(int forecastID)
{
    //log
    ofstream info;
    Lib::initInfoLogHandle(_logPath, info);
    info << "TradeLogicSrv[sendRollBack]";
    info << "|iID|" << _instrumnetID;
    info << "|forecastID|" << forecastID;
    info << endl;
    info.close();

    MSG_TO_TRADE_STRATEGY msg = {0};
    msg.msgType = MSG_TRADE_ROLLBACK;
    msg.forecastID = forecastID;
    msg.isMain = true;
    msg.isForecast = false;
    strcpy(msg.instrumnetID, Lib::stoc(_instrumnetID));
    _tradeStrategySrvClient->send((void *)&msg);

}
