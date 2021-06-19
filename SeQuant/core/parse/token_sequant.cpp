//
// Created by Bimal Gaudel on 6/15/21.
//

#include "token_sequant.hpp"

namespace sequant::parse {

enum {PrecedencePlusOrMinus,PrecedenceTimes };

int OperatorTimes::precedence() const { return PrecedenceTimes; }

Token::type_id_type OperatorTimes::type_id() const {
  return Token::get_type_id<OperatorTimes>();
}

int OperatorPlus::precedence() const { return PrecedencePlusOrMinus; }

Token::type_id_type OperatorPlus::type_id() const {
  return Token::get_type_id<OperatorPlus>();
}

int OperatorMinus::precedence() const { return PrecedencePlusOrMinus; }

Token::type_id_type OperatorMinus::type_id() const {
  return Token::get_type_id<OperatorMinus>();
}

Token::type_id_type OperandConstant::type_id() const {
  return Token::get_type_id<OperandConstant>();
}

Token::type_id_type OperandTensor::type_id() const {
  return Token::get_type_id<OperandTensor>();
}

} // namespace