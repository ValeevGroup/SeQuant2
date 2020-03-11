#include "eval_tensor.hpp"
#include "eval_tensor_fwd.hpp"

#include <boost/any.hpp>
#include <memory>

namespace sequant::evaluate {

// EvalTensor the base class
void EvalTensor::set_indices(const IndexLabelContainer& index_labels) {
  indices_ = index_labels;
}

const IndexLabelContainer& EvalTensor::get_indices() const { return indices_; }

void EvalTensor::set_hash_value(HashType hash_value) {
  hash_value_ = hash_value;
}

HashType EvalTensor::get_hash_value() const { return hash_value_; }

void EvalTensor::set_ops_count(OpsCount count) { ops_count_ = count; }

OpsCount EvalTensor::get_ops_count() const { return ops_count_; }

ScalarType EvalTensor::get_scalar() const { return scalar_; }

void EvalTensor::set_scalar(ScalarType scale) { scalar_ = scale; }

// EvalTensorIntermediate
bool EvalTensorIntermediate::is_leaf() const { return false; }

void EvalTensorIntermediate::set_left_tensor(
    const std::shared_ptr<EvalTensor>& tensor_ptr) {
  left_tensor_ = tensor_ptr;
}

const EvalTensorPtr& EvalTensorIntermediate::get_left_tensor() const {
  return left_tensor_;
}

void EvalTensorIntermediate::set_right_tensor(
    const std::shared_ptr<EvalTensor>& tensor_ptr) {
  right_tensor_ = tensor_ptr;
}

const EvalTensorPtr& EvalTensorIntermediate::get_right_tensor() const {
  return right_tensor_;
}

void EvalTensorIntermediate::set_operation(Operation op) { operation_ = op; }

Operation EvalTensorIntermediate::get_operation() const { return operation_; }

// EvalTensorLeaf
void EvalTensorLeaf::set_data_tensor(
    const std::shared_ptr<boost::any>& dtensor_ptr) {
  data_tensor_ = dtensor_ptr;
}

bool EvalTensorLeaf::is_leaf() const { return true; }

}  // namespace sequant::evaluate
