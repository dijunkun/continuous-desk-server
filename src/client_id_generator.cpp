#include "client_id_generator.h"

ClientIdGenerator::ClientIdGenerator() {}

ClientIdGenerator::~ClientIdGenerator() {}

std::string ClientIdGenerator::GeneratorNewId() {
  return std::to_string(++base_id_);
}