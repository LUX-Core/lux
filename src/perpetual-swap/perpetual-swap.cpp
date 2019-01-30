#include "order.cpp";
#include "account.cpp";

namespace ContractPerpetualSwap {
    struct EventReportOrder{ //????
        address  _sender;
        uint256 _price;
        int256 _amount;
    };
   EventReportOrder ReportAccount(address,uint80,uint,uint256,uint256); //????
};

using namespace ContractPerpetualSwap;
class ContractPerpetualSwap  {
    
    public:

    order::OrderPool bidOrderPool;
    order::OrderPool sellOrderPool;

    account::AccountPool accountPool;

    EventReportOrder reportAccount(address _address) 
    {
        account::Account account = account::getAccount(accountPool,_address);
        EventReportOrder report=ReportAccount(account.accountOwner,account.index,account.position,
                                       account.price,account.amount);
        return report;
    }
 
  
    void addOrder(address _address, uint _position, uint256 _price, uint256 _amount) 
    {
        auto account = account::getAccount(accountPool,_address);
        if(account.position == _position)
        {
            auto total = account.price * account.amount + _price * _amount;
            account.amount = account.amount + _amount;
            account.price = total/account.amount;
        }
        else
        {
            if(account.amount == _amount)
            {
                account.position = 0; //Null
                account.price = 0;
                account.amount = 0;
            }
            else if(account.amount < _amount)
            {
                account.position = _position;
                account.price = _price;
                account.amount = _amount - account.amount;
                //to do: adjust margin pool
            }
            else
            {
                account.amount = account.amount - _amount;
                ///to do: adjust margin pool
             }
        }
        updateAccount(accountPool, account);
    }
    
    void setBidOrder(uint256 _price, uint256 _amount)
    {
        auto bidAmount = _amount;
        if(sellOrderPool.count != 0)
        {
            auto order = startOrder(sellOrderPool);
            //try to make deals as more as posible
            while(_price >= order.data.price)
            {
                //make the deal
                if(bidAmount > order.data.amount)
                {
                    bidAmount = bidAmount - order.data.amount;
                    //order.data.amount have been dealed, add states change here
                    addOrder(msg.sender,1,order.data.price,order.data.amount);  //msg.sender Long position
                    addOrder(order.orderOwner,2,order.data.price,order.data.amount);  //orderOwner Short position
                    
                    reportAccount(order.orderOwner);
                    
                    
                    order::remove(sellOrderPool,order.curr);
                    
                    if(sellOrderPool.count == 0)
                    {
                        //no sell order in the market
                        break;
                    }
                    else
                    {
                        //iterate to next sell order and continue
                        order = startOrder(sellOrderPool);
                        continue;
                    }
                }
                else
                {
                    order.data.amount = order.data.amount - bidAmount;
                    bidAmount = 0;
                    //bidAmount have been dealed, add states change here
                    addOrder(msg.sender,1,order.data.price,bidAmount);  //msg.sender Long position
                    addOrder(order.orderOwner,2,order.data.price,bidAmount);  //orderOwner Short position
                    reportAccount(order.orderOwner);
                    
                    if(order.data.amount == 0)
                    {
                        order::remove(sellOrderPool,order.curr);
                    }
                    else
                    {    
                        order::updateOrder(sellOrderPool,order);
                    }
                    break;
                }
            }
        }
        
        if(bidAmount > 0)
        {
            order::Data data = {_price, bidAmount};
            forwardAdd(bidOrderPool,_price, data);
        }
        reportAccount(msg.sender);
    }
    
    void setSellOrder(uint256 _price, uint256 _amount)
    {
        auto sellAmount = _amount;
        if(bidOrderPool.count != 0)
        {
            auto order = endOrder(bidOrderPool);
            //try to make deals as more as posible
            while(_price <= order.data.price)
            {
                if(sellAmount > order.data.amount)
                {
                    sellAmount = sellAmount - order.data.amount;
                    //order.data.amount have been dealed, add states change here
                    addOrder(order.orderOwner,1,order.data.price,order.data.amount);  //orderOwner Long position
                    addOrder(msg.sender,2,order.data.price,order.data.amount);  //msg.sender Short position
                    reportAccount(order.orderOwner);
                    
                    order::remove(bidOrderPool,order.curr);
                    
                    if(bidOrderPool.count == 0)
                    {
                        //no bid order in the market
                        break;
                    }
                    else
                    {
                        //iterate to next bid order and continue
                        order = endOrder(bidOrderPool);
                        continue;
                    }
                }
                else
                {
                    order.data.amount = order.data.amount - sellAmount;
                    sellAmount = 0;
                    //bidAmount have been dealed, add states change here
                    addOrder(order.orderOwner,1,order.data.price,sellAmount);  //orderOwner Long position
                    addOrder(msg.sender,2,order.data.price,sellAmount);  //msg.sender Short position
                    reportAccount(order.orderOwner);
                    
                    if(order.data.amount == 0)
                    {
                        remove(bidOrderPool,order.curr);
                    }
                    else
                    {
                        updateOrder(bidOrderPool,order);
                    }
                    break;
                }
            }
        }
        
        if(sellAmount > 0)
        {
            order::Data data = {_price, sellAmount};
            backwardAdd(sellOrderPool, _price, data);
        }
        reportAccount(msg.sender);
    }
};
