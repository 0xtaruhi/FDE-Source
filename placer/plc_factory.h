#ifndef PLCFACTORY_H
#define PLCFACTORY_H

#include "netlist.hpp"
#include "plc_boundingbox.h"
#include "plc_device.h"
#include "tnetlist.hpp"

namespace FDU {
namespace Place {

// using COS::NLList;
// using COS::NLContainer;
using namespace COS;
//  	using COS::Instance;
//  	using COS::Net;
//  	using COS::Module;
//  	using COS::Library;
// using COS::NLFactory;

/************************************************************************/
/* 标示布局中所用instance的类                                           */
/************************************************************************/
class PLCInstance : public Instance {
public:
  PLCInstance(const std::string &name, Module &instof, Module &owner)
      : Instance(name, &instof, &owner), _curr_loc_site(NULL),
        _curr_logic_pos(), _is_fixed(false), _swapable_type(FLOORPLAN::SITE) {}
  // 设定该instance的从属site，这里保存了两个指针，一个是原来的，一个是当前的
  void set_curr_loc_site(Site *site) {
    _past_loc_site = _curr_loc_site;
    _curr_loc_site = site;
  }
  // 得到当前的site
  Site *curr_loc_site() const { return _curr_loc_site; }
  // 得到以前的site
  Site *past_loc_site() const { return _past_loc_site; }
  // 复位site
  void reset_loc_site() { _curr_loc_site = _past_loc_site; }

  // 设置当前逻辑地址
  void set_curr_logic_pos(const Point &pos) {
    _past_logic_pos = _curr_logic_pos;
    _curr_logic_pos = pos;
  }
  // 得到当前/以前逻辑地址
  Point curr_logic_pos() const { return _curr_logic_pos; }
  Point past_logic_pos() const { return _past_logic_pos; }
  // 复位逻辑地址
  void reset_logic_pos() { _curr_logic_pos = _past_logic_pos; }

  // fix代表约束文件中固定的Instance
  void fix_inst() { _is_fixed = true; }
  void loose_inst() { _is_fixed = false; }
  bool is_fixed() const { return _is_fixed; }

  Site::SiteType site_type() const { return _curr_loc_site->_type; }
  // 设置/得到该isntance的可交换类型
  void set_swapable_type(FLOORPLAN::SwapableType t) { _swapable_type = t; }
  FLOORPLAN::SwapableType swapable_type() const { return _swapable_type; }
  // 判断是否是macro type
  bool is_macro() const {
    return _swapable_type == FLOORPLAN::LUT6 ||
           _swapable_type == FLOORPLAN::CARRY_CHAIN;
  }

private:
  // 存储上一个和当前从属的site
  Site *_curr_loc_site, *_past_loc_site;
  // 存储当前和上一个的逻辑地址
  Point _curr_logic_pos, _past_logic_pos;
  // 是否是fix
  bool _is_fixed;
  // 该instance的可交换类型
  FLOORPLAN::SwapableType _swapable_type;
};

/************************************************************************/
/* 标示布局中所用net的类                                                */
/************************************************************************/
class PLCNet : public Net {
public:
  enum AffectedType { NONE = 0, FROM = 1, TO = 2 };

  PLCNet(const std::string &name, COS::NetType type, Module &owner)
      : Net(name, type, &owner, NULL), _is_ignored(false), _affected_type(NONE),
        _bounding_box(new BoundingBox(this)), _total_tcost(0.) {}
  ~PLCNet() { delete _bounding_box; }
  // 设置/得到net为ignored
  void set_ignored() { _is_ignored = true; }
  bool is_ignored() const { return _is_ignored; }

  void save_bounding_box() { _bounding_box->save_bounding_box(); }
  void restore_bounding_box() { _bounding_box->restore_bounding_box(); }

  double compute_tcost(double dmax, double crit_exp);
  double get_total_tcost() const { return _total_tcost; }

  void set_pre_bb_cost(double cost) { _bounding_box->set_pre_bb_cost(cost); }
  void reset_pre_bb_cost() { _bounding_box->reset_pre_bb_cost(); }
  double pre_bb_cost() const { return _bounding_box->pre_bb_cost(); }
  double bb_cost() const { return _bounding_box->bb_cost(); }
  double compute_bb_cost(double inv_of_chan_width) {
    return _bounding_box->compute_bb_cost(inv_of_chan_width);
  }

  void compute_bounding_box(const Point &device_scale) {
    _bounding_box->compute_bounding_box(device_scale);
  }
  void update_bounding_box(const Point &pos_from, const Point &pos_to,
                           const Point &device_scale) {
    _bounding_box->update_bounding_box(device_scale, pos_from, pos_to);
  }
  // 设置受影响线网的类型
  void set_affected_type(AffectedType type) { _affected_type |= type; }
  void reset_affected_type() { _affected_type = 0; }
  int affected_type() const { return _affected_type; }

  typedef std::vector<PLCInstance *> ConnectedInsts;
  const ConnectedInsts &connected_insts() const { return _connected_insts; }
  int num_connected_insts() const { return _connected_insts.size(); }
  void store_connected_insts();

private:
  bool _is_ignored;
  int _affected_type;
  BoundingBox *_bounding_box;

  double _total_tcost;

  ConnectedInsts _connected_insts;
};

class PLCModule : public Module {
public:
  PLCModule(const std::string &name, const std::string &type, Library *owner)
      : Module(name, type, owner) {}

  typedef PtrList<Instance>::typed<PLCInstance>::range instances_type;
  typedef PtrList<Instance>::typed<PLCInstance>::const_range
      const_instances_type;
  typedef PtrList<Instance>::typed<PLCInstance>::iterator instance_iter;
  typedef PtrList<Instance>::typed<PLCInstance>::const_iterator
      const_instance_iter;

  instances_type instances() {
    return static_cast<instances_type>(Module::instances());
  }
  const_instances_type instances() const {
    return static_cast<const_instances_type>(Module::instances());
  }

  typedef PtrVector<Net>::typed<PLCNet>::range nets_type;
  typedef PtrVector<Net>::typed<PLCNet>::const_range const_nets_type;
  typedef PtrVector<Net>::typed<PLCNet>::iterator net_iter;
  typedef PtrVector<Net>::typed<PLCNet>::const_iterator const_net_iter;

  nets_type nets() { return static_cast<nets_type>(Module::nets()); }
  const_nets_type nets() const {
    return static_cast<const_nets_type>(Module::nets());
  }
};

/************************************************************************/
/* 产生布局所需元素的工厂 */
/************************************************************************/
class PLCFactory : public TimingFactory {
public:
  ~PLCFactory() {}

  virtual Module *make_module(const string &name, const string &type,
                              Library *owner) {
    return new PLCModule(name, type, owner);
  }

  virtual Instance *make_instance(const string &name, Module *down_module,
                                  Module *owner) {
    return new PLCInstance(name, *down_module, *owner);
  }

  virtual Net *make_net(const string &name, NetType type, Module *owner,
                        Bus *bus) {
    return new PLCNet(name, type, *owner);
  }
};

} // namespace Place
} // namespace FDU

#endif