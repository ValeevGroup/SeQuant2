//
// Created by Bimal Gaudel on 12/26/19.
//

#include "path_tree.hpp"

#include <SeQuant/core/container.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace sequant::evaluate {

PathTree::PathTree(size_t x) : label_{x} { children_.clear(); }

size_t PathTree::get_label() const { return label_; }

void PathTree::add_child(PathTreePtr& ptr) { children_.push_back(ptr); }

const sequant::container::svector<PathTreePtr>& PathTree::get_children() const {
  return children_;
}
sequant::container::svector<PathTreePtr>& PathTree::get_children() {
  return children_;
}

void PathTree::pop_last_child() { children_.pop_back(); }

bool PathTree::is_leaf() const { return children_.empty(); }

std::wstring PathTree::print_tree() const {
  if (is_leaf()) {
    // if  this is a leaf, return a wstring
    return L" " + std::to_wstring(get_label());
  } else {
    // else recursively get wstring for the children
    // and append them to the wstring of this node before returning
    std::wstring result = L" (" + std::to_wstring(get_label());
    for (const auto& ch : get_children()) {
      result += ch->print_tree();
    }
    result += L")";
    return result;
  }
}

}  // namespace sequant::evaluate
