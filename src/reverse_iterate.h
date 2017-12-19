// Copyright (c) 2017 The LUX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
/*
    Template used for reverse iteration in C++11 range-based for loops.
    std::vector<int> v = {1, 2, 3, 4, 5};
    for (auto x : reverse_iterate(v))
        std::cout << x << " ";
 */

template <typename T>
class reverse_range
{
    T &x;
    
public:
    reverse_range(T &x) : x(x) {}
    
    auto begin() const -> decltype(this->x.rbegin())
    {
        return x.rbegin();
    }
    
    auto end() const -> decltype(this->x.rend())
    {
        return x.rend();
    }
};
 
template <typename T>
reverse_range<T> reverse_iterate(T &x)
{
    return reverse_range<T>(x);
}

