#include "myTypes.hpp"

namespace account
{

    struct AccountPool
    {
        map<address,uint80> indexMap;
        vector<Account> pool;
    };
    
    struct Account
    {
        address accountOwner;
        uint80 index;
        uint position;   //Null:0, Long:1, Short:2
        uint256 price;
        uint256 amount;
    };
    
    Account getAccount(AccountPool  self, address _address) 
    {
        if(self.indexMap[_address] == 0)
        {
            

            Account account = {msg.sender,0,0,0,0};
            self.pool.push_back(account);
            auto index = self.pool.size()-1;

            self.indexMap[msg.sender] = index;

            self.pool[index-1].index = index;
            account.index = index;
            return account;
        }
        else
        {
            Account account  = self.pool[self.indexMap[_address]-1];
            return account;
        }
    }
    
    void updateAccount(AccountPool  self, Account _account) 
    {
        self.pool[_account.index-1].position = _account.position;
        self.pool[_account.index-1].price = _account.price;
        self.pool[_account.index-1].amount = _account.amount;
    }
};

// How to use it:
using namespace account;
class Contractaccount {
    
    public:

    account::AccountPool accountPool;
    void addOrder(uint _position, uint256 _price, uint256 _amount)
    {
        auto account = getAccount(accountPool, msg.sender);
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
        updateAccount(accountPool,account);
    }
} TestAccountPool;