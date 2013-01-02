/***************************************************************************************************
 *  Copyright 2012 MaidSafe.net limited                                                            *
 *                                                                                                 *
 *  The following source code is property of MaidSafe.net limited and is not meant for external    *
 *  use.  The use of this code is governed by the licence file licence.txt found in the root of    *
 *  this directory and also on www.maidsafe.net.                                                   *
 *                                                                                                 *
 *  You are not free to copy, amend or otherwise use this source code without the explicit         *
 *  written permission of the board of directors of MaidSafe.net.                                  *
 **************************************************************************************************/

#ifndef MAIDSAFE_VAULT_DATA_HOLDER_H_
#define MAIDSAFE_VAULT_DATA_HOLDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/filesystem/path.hpp"
#include <boost/graph/graph_concepts.hpp>

#include "maidsafe/common/log.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/api_config.h"

#include "maidsafe/vault/get_policies.h"
#include "maidsafe/vault/post_policies.h"
#include "maidsafe/vault/delete_policies.h"
#include "maidsafe/vault/put_policies.h"

#include "maidsafe/nfs/nfs.h"

#include "maidsafe/data_store/data_store.h"

#include "maidsafe/vault/utils.h"

namespace maidsafe {

namespace routing { class Routing; }
namespace nfs { class Message; }


namespace vault {

namespace test { class DataHolderTest; }


//typedef nfs::NetworkFileSystem<NoGet, NoPut, NoPost, NoDelete> DataHolderNfs;

class DataHolder {
 public:
  DataHolder(const boost::filesystem::path& vault_root_dir);
  ~DataHolder();

  template <typename Data>
  void HandleMessage(const nfs::Message& message, const routing::ReplyFunctor& reply_functor);
  template <typename Data>
  bool IsInCache(nfs::Message& message);
  template <typename Data>
  void StoreInCache(const nfs::Message& message);
  void StopSending();
  void ResumeSending();

 private:
  friend class test::DataHolderTest;
  template <typename Data>
  void HandlePutMessage(const nfs::Message& message, const routing::ReplyFunctor& reply_functor);
  template <typename Data>
  void HandleGetMessage(const nfs::Message& message, const routing::ReplyFunctor& reply_functor);
  template <typename Data>
  void HandlePostMessage(const nfs::Message& message, const routing::ReplyFunctor& reply_functor);
  template <typename Data>
  void HandleDeleteMessage(const nfs::Message& message, const routing::ReplyFunctor& reply_functor);

  boost::filesystem::space_info space_info_;
  DiskUsage disk_total_;
  DiskUsage permanent_size_;
  DiskUsage cache_size_;

  boost::filesystem::path persona_dir_;
  boost::filesystem::path persona_dir_permanent_;
  boost::filesystem::path persona_dir_cache_;
  data_store::DataStore<data_store::DataBuffer> permanent_data_store_;
  data_store::DataStore<data_store::DataBuffer> cache_data_store_;
  data_store::DataStore<data_store::DataBuffer> mem_only_cache_;
  std::atomic<bool> stop_sending_;
};

template <typename Data>
void DataHolder::HandleMessage(const nfs::Message& message,
                               const routing::ReplyFunctor& reply_functor) {
  LOG(kInfo) << "received message at Data holder";
  switch (message.action_type()) {
    case nfs::ActionType::kGet :
      HandleGetMessage<Data>(message, reply_functor);
      break;
    case nfs::ActionType::kPut :
      HandlePutMessage<Data>(message, reply_functor);
      break;
    case nfs::ActionType::kPost :
      HandlePostMessage<Data>(message, reply_functor);
      break;
    case nfs::ActionType::kDelete :
      HandleDeleteMessage<Data>(message, reply_functor);
      break;
    default :
      LOG(kError) << "Unhandled action type";
  }
}

template <typename Data>
void DataHolder::HandleGetMessage(const nfs::Message& message,
                                  const routing::ReplyFunctor& reply_functor) {
  try {
    NonEmptyString result(
        cache_data_store_.Get(typename Data::name_type(
                                  Identity(message.destination()->node_id.string()))));
    std::string string(result.string());
    reply_functor(string);
  } catch (std::exception& /*ex*/) {
    //reply_functor(nfs::ReturnCode(-1).Serialise().data.string()); // non 0 plus optional message
    reply_functor("");
    // error code // at the moment this will go back to client
    // in production it will g back to
  }
}

template <typename Data>
void DataHolder::HandlePutMessage(const nfs::Message& message,
                                  const routing::ReplyFunctor& reply_functor) {
  try {
    permanent_data_store_.Store(typename Data::name_type(
                                    Identity(message.destination()->node_id.string())),
                                message.content());
    reply_functor(nfs::ReturnCode(0).Serialise().data.string());
  } catch (std::exception& /*ex*/) {
    reply_functor(nfs::ReturnCode(-1).Serialise().data.string()); // non 0 plus optional message
    // error code // at the moment this will go back to client
    // in production it will g back to
  }
}

template <typename Data>
void DataHolder::HandlePostMessage(const nfs::Message& /*message*/,
                                   const routing::ReplyFunctor& /*reply_functor*/) {
// no op
}

template <typename Data>
void DataHolder::HandleDeleteMessage(const nfs::Message& /*message*/,
                                     const routing::ReplyFunctor& /*reply_functor*/) {
//  permenent_data_store.Delete(message.data_type() message.content().name());
}

}  // namespace vault

}  // namespace maidsafe

#endif  // MAIDSAFE_VAULT_DATA_HOLDER_H_
