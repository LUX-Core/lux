   
   #include "myTypes.hpp"

   namespace order{
      
    const uint80  None = uint80(0);

   

    struct OrderPool {
        uint80 first;
        uint80 last;
        uint80 count;
        vector<Order> pool;
    };
    struct Order {
        uint80 prev;
        uint80 curr;
        uint80 next;
        uint256 key;
        address orderOwner;
        bool valid;
        Data data;
    };
    //Adding data members here
    struct Data {
        uint256 price;
        uint256 amount;
    };

    
    Order startOrder(OrderPool self) 
    {
        return self.pool[self.first-1];
    }

    
    Order endOrder(OrderPool  self) 
    {
        return self.pool[self.last-1];
    }
    
    //update the data in the order struct
    void updateOrder(OrderPool self, Order order) 
    {
        if(iterateValid(self, order.curr))
        {
            self.pool[order.curr-1].data = order.data;
        }
    }
    
    void forwardAdd(OrderPool self, uint256 _key, Data _data) 
    {
        auto it = forwardSearch(self, _key);
        if(None == it)
        {
            startAppend(self, _key, _data);
        }
        else
        {
            frontInsert(self, it, _key, _data);
        }
    }
    
    void backwardAdd(OrderPool self, uint256 _key, Data _data) 
    {
        auto it = backwardSearch(self, _key);
        if(None == it)
        {
            endAppend(self, _key, _data);
        }
        else
        {
            backInsert(self, it, _key, _data);
        }
    }
    
    /// Removes the element identified by the iterator
    /// `_index` from the list `self`.
    bool remove(OrderPool self, uint80 _index) 
    {
        Order order = self.pool[_index - 1];
        if(order.valid == false)
        {
            //this order has already been deleted
            return true;
        }
        if(msg.sender != order.orderOwner)
        {
            //no permission to do this
            return false;
        }
        if (order.prev == None)
            self.first = order.next;
        if (order.next == None)
            self.last = order.prev;
        if (order.prev != None)
            self.pool[order.prev - 1].next = order.next;
        if (order.next != None)
            self.pool[order.next - 1].prev = order.prev;
        
        //delete self.pool[_index - 1];
        self.pool.erase(self.pool.begin() + _index-1);
        self.count--;
    }

//------------------------------------------private function-------------------------------------------------
    
    /// Appends `_data` to the front of the list `self`.
    void startAppend(OrderPool self, uint256 _key, Data _data) 
    {
         uint80 prev;
        uint80 curr;
        uint80 next;
        uint256 key;
        address orderOwner;
        bool valid;
        Data data;
        
        
        Order order = {None, None, self.first, _key, msg.sender, true, _data};
        self.pool.push_back(order);
        auto index = self.pool.size()-1;

        self.pool[index - 1].curr = index;
        if (self.first == None)
        {
            if (self.last != None || self.count != 0) throw;
            self.first = self.last = index;
            self.count = 1;
        }
        else
        {
            self.pool[self.first - 1].prev = index;
            self.first = index;
            self.count ++;
        }
    }
    
    /// Appends `_data` to the end of the list `self`.
   void endAppend(OrderPool self, uint256 _key, Data _data) 
    {
        Order order = {self.last, None, None, _key, msg.sender, true, _data};
        
        self.pool.push_back(order);
        auto index = self.pool.size()-1;

        self.pool[index - 1].curr = index;
        if (self.last == None)
        {
            if (self.first != None || self.count != 0) throw;
            self.first = self.last = index;
            self.count = 1;
        }
        else
        {
            self.pool[self.last - 1].next = index;
            self.last = index;
            self.count ++;
        }
    }
    
    // Caller must guarantee the index item valid and in the list
   void frontInsert(OrderPool self, uint80 _index, uint256 _key, Data _data) 
    {
        auto prevIndex = self.pool[_index - 1].prev;
        Order order = {None, None, None, _key, msg.sender, true, _data};
        
        self.pool.push_back(order);
        auto newIndex = self.pool.size()-1;

        self.pool[newIndex - 1].prev = prevIndex;
        self.pool[newIndex - 1].curr = newIndex;
        self.pool[newIndex - 1].next = _index;

        self.pool[_index - 1].prev = newIndex;
        
        if(prevIndex != None)
        {
            self.pool[prevIndex - 1].next = newIndex;
        }
        else
        {
            self.first = newIndex;
        }
        
        self.count ++;
    }
    
    // Caller must guarantee the index item valid and in the list
    void backInsert(OrderPool self, uint80 _index, uint256 _key, Data _data) 
    {
        auto nextIndex = self.pool[_index - 1].next;

        Order order = {None, None, None, _key, msg.sender, true, _data};
        
        self.pool.push_back(order);
        auto newIndex = self.pool.size()-1;

        
        self.pool[newIndex - 1].prev = _index;
        self.pool[newIndex - 1].curr = newIndex;
        self.pool[newIndex - 1].next = nextIndex;
        
        self.pool[_index - 1].next = newIndex;
        
        if(nextIndex != None)
        {
            self.pool[nextIndex - 1].prev = newIndex;
        }
        else
        {
            self.last = newIndex;
        }
        
        self.count ++;
    }
    
    //forward search until the value of item equal or bigger than _price
    //if it == null, iterating is end
    uint80 forwardSearch(OrderPool  self, uint256 _key) 
    {
        uint80 it = iterateFirst(self);
        while (iterateValid(self, it)) {
            if (iterateGet(self, it) >= _key)
                return it;
            it = iterateNext(self, it);
        }
        return it;
    }
    
    //backward search until the value of item equal or smaller than _price
    //if it == null, iterating is end
    uint80 backwardSearch(OrderPool  self, uint256 _key) 
    {
        auto it = iterateLast(self);
        while (iterateValid(self, it)) {
            if (iterateGet(self, it) <= _key)
                return it;
            it = iteratePrev(self, it);
        }
        return it;
    }
    
    // Iterator interface
    uint80  iterateFirst(OrderPool self) 
    { 
        return self.first; 
    }
    uint80 iterateLast( OrderPool  self) 
    { 
        return self.last; 
    }
    
    bool iterateValid( OrderPool  self,  uint80 _index) 
    {
        return _index - 1 < self.pool.size(); 
    }

    uint80 iteratePrev(OrderPool  self,  uint80 _index) 
    { 
        return self.pool[_index - 1].prev; 
    }

    uint80 iterateNext(OrderPool  self,  uint80 _index) 
    { 
        return self.pool[_index - 1].next; 
    }
    uint256 iterateGet(OrderPool  self,  uint80 _index) 
    { 
        return self.pool[_index - 1].key; 
    }
};

using namespace order;
class Contractorder {
    
    public:

    order::OrderPool bidOrderPool;
    order::OrderPool sellOrderPool;

    void updateOrder()
    {
        Order order = endOrder(bidOrderPool);
        order.data.amount = 123;
        order::updateOrder(bidOrderPool, order);
        
        order = startOrder(sellOrderPool);
        order.data.amount = 12;
        order::updateOrder(sellOrderPool, order);
    }
    
    void setBidOrder(uint256 _price, uint256 _amount)
    {
        Data data = {_price, _amount};
        forwardAdd(bidOrderPool,_price, data);
    }
    
    void setSellOrder(uint256 _price, uint256 _amount)
    {
        Data data = {_price, _amount};
        backwardAdd(sellOrderPool,_price, data);
    }
    
    void remove(uint80 _index)
    {
        order::remove(bidOrderPool, _index);
        order::remove(sellOrderPool,_index);
    }
}TestOrderPool;
