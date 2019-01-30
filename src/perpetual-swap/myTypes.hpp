//
//  myTypes.hpp
//  
//
//  Created by 1 on 1/30/19.
//

#ifndef myTypes_hpp
#define myTypes_hpp

#include <cstdint>
   #include <vector>
   #include <string>
   #include <map>
   #include <boost/multiprecision/cpp_int.hpp>

   using namespace std;
   using namespace boost::multiprecision;

   typedef  uint64_t uint80; //?????
   typedef  uint256_t uint256;
   typedef  int256_t int256;
   typedef  string address;

    struct Address { //?????????
        string sender;
    }msg; 


#endif /* myTypes_hpp */
