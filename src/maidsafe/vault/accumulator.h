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

#ifndef MAIDSAFE_VAULT_ACCUMULATOR_H_
#define MAIDSAFE_VAULT_ACCUMULATOR_H_

#include <deque>
#include <utility>
#include <vector>

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/nfs/data_message.h"
#include "maidsafe/nfs/reply.h"
#include "maidsafe/nfs/types.h"
#include "maidsafe/nfs/utils.h"


namespace maidsafe {

namespace vault {

namespace test {
class AccumulatorTest_BEH_PushRequest_Test;
class AccumulatorTest_BEH_PushRequestThreaded_Test;
class AccumulatorTest_BEH_CheckPendingRequestsLimit_Test;
class AccumulatorTest_BEH_CheckHandled_Test;
class AccumulatorTest_BEH_SetHandled_Test;
}  // namespace test

template<typename Name>
class Accumulator {
 public:
  struct PendingRequest {
    PendingRequest(const nfs::DataMessage& msg_in,
                   const routing::ReplyFunctor& reply_functor_in,
                   const nfs::Reply& reply_in);
    PendingRequest(const PendingRequest& other);
    PendingRequest& operator=(const PendingRequest& other);
    PendingRequest(PendingRequest&& other);
    PendingRequest& operator=(PendingRequest&& other);

    nfs::DataMessage msg;
    routing::ReplyFunctor reply_functor;
    nfs::Reply reply;
  };

  struct HandledRequest {
    HandledRequest(const nfs::MessageId& msg_id_in,
                   const Name& source_name_in,
                   const nfs::DataMessage::Action& action_type_in,
                   const Identity& data_name,
                   const DataTagValue& data_type,
                   const int32_t& size_in,
                   const nfs::Reply& reply_in);
    HandledRequest(const HandledRequest& other);
    HandledRequest& operator=(const HandledRequest& other);
    HandledRequest(HandledRequest&& other);
    HandledRequest& operator=(HandledRequest&& other);

    nfs::MessageId msg_id;
    Name source_name;
    nfs::DataMessage::Action action;
    Identity data_name;
    DataTagValue data_type;
    int32_t size;
    nfs::Reply reply;
  };

  typedef TaggedValue<NonEmptyString, struct SerialisedRequestsTag> serialised_requests;

  Accumulator();

  // Returns true and populates <reply_out> if the message has already been set as handled.
  bool CheckHandled(const nfs::DataMessage& data_message, nfs::Reply& reply_out) const;
  // Adds a request with its individual result pending the overall result of the operation.
  std::vector<nfs::Reply> PushRequest(const nfs::DataMessage& data_message,
                                      const routing::ReplyFunctor& reply_functor,
                                      const nfs::Reply& reply);
  // Marks the message as handled and returns all pending requests held with the same ID
  std::vector<PendingRequest> SetHandled(const nfs::DataMessage& data_message,
                                         const nfs::Reply& reply);
  // Returns all handled requests for the given account name.
  std::vector<HandledRequest> Get(const Name& name) const;
  // Serialises all handled requests for the given account name.
  serialised_requests Serialise(const Name& name) const;
  // Parses the list of serialised handled requests.
  std::vector<HandledRequest> Parse(const serialised_requests& serialised_requests_in) const;

  friend class test::AccumulatorTest_BEH_PushRequest_Test;
  friend class test::AccumulatorTest_BEH_PushRequestThreaded_Test;
  friend class test::AccumulatorTest_BEH_CheckPendingRequestsLimit_Test;
  friend class test::AccumulatorTest_BEH_CheckHandled_Test;
  friend class test::AccumulatorTest_BEH_SetHandled_Test;

 private:
  Accumulator(const Accumulator&);
  Accumulator& operator=(const Accumulator&);
  Accumulator(Accumulator&&);
  Accumulator& operator=(Accumulator&&);

  std::deque<PendingRequest> pending_requests_;
  std::deque<HandledRequest> handled_requests_;
  const size_t kMaxPendingRequestsCount_, kMaxHandledRequestsCount_;
};

}  // namespace vault

}  // namespace maidsafe

#include "maidsafe/vault/accumulator-inl.h"

#endif  // MAIDSAFE_VAULT_ACCUMULATOR_H_