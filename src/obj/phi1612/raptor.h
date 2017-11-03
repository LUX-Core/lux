#ifndef RAPTOR_H
#define RAPTOR_H

class RaptorFuck{


  unsigned char keyStream[16];
  unsigned long long int blockCounter;
  int byteCounter;;
  
  unsigned long long int nonce;
  
  
 public:
  
  RaptorFuck();

  int keySize();
  
  void setNonce( unsigned long long int nonce );
  
  void setKey(unsigned char * key);
  
  unsigned int encrypt(unsigned int byte);
  
  
};

#endif