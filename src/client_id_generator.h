/*
 * @Author: DI JUNKUN
 * @Date: 2024-08-06
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _CLIENT_ID_GENERATOR_H_
#define _CLIENT_ID_GENERATOR_H_

#include <iostream>

class ClientIdGenerator {
 public:
  ClientIdGenerator();
  ~ClientIdGenerator();

 public:
  std::string GeneratorNewId();

 private:
  int base_id_ = 300000000;
};

#endif