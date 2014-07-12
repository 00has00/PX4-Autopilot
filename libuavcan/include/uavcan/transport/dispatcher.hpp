/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#pragma once

#include <cassert>
#include <uavcan/error.hpp>
#include <uavcan/stdint.hpp>
#include <uavcan/impl_constants.hpp>
#include <uavcan/transport/perf_counter.hpp>
#include <uavcan/transport/transfer_listener.hpp>
#include <uavcan/transport/outgoing_transfer_registry.hpp>
#include <uavcan/transport/can_io.hpp>
#include <uavcan/util/linked_list.hpp>

namespace uavcan
{

class UAVCAN_EXPORT Dispatcher;

class UAVCAN_EXPORT LoopbackFrameListenerBase : public LinkedListNode<LoopbackFrameListenerBase>, Noncopyable
{
    Dispatcher& dispatcher_;

protected:
    explicit LoopbackFrameListenerBase(Dispatcher& dispatcher)
        : dispatcher_(dispatcher)
    { }

    virtual ~LoopbackFrameListenerBase() { stopListening(); }

    void startListening();
    void stopListening();
    bool isListening() const;

    Dispatcher& getDispatcher() { return dispatcher_; }

public:
    virtual void handleLoopbackFrame(const RxFrame& frame) = 0;
};


class UAVCAN_EXPORT LoopbackFrameListenerRegistry : Noncopyable
{
    LinkedListRoot<LoopbackFrameListenerBase> listeners_;

public:
    void add(LoopbackFrameListenerBase* listener);
    void remove(LoopbackFrameListenerBase* listener);
    bool doesExist(const LoopbackFrameListenerBase* listener) const;
    unsigned getNumListeners() const { return listeners_.getLength(); }

    void invokeListeners(RxFrame& frame);
};


class UAVCAN_EXPORT Dispatcher : Noncopyable
{
    CanIOManager canio_;
    ISystemClock& sysclock_;
    IOutgoingTransferRegistry& outgoing_transfer_reg_;
    TransferPerfCounter perf_;

    class ListenerRegistry
    {
        LinkedListRoot<TransferListenerBase> list_;

        class DataTypeIDInsertionComparator
        {
            const DataTypeID id_;
        public:
            explicit DataTypeIDInsertionComparator(DataTypeID id) : id_(id) { }
            bool operator()(const TransferListenerBase* listener) const
            {
                assert(listener);
                return id_ > listener->getDataTypeDescriptor().getID();
            }
        };

    public:
        enum Mode { UniqueListener, ManyListeners };

        bool add(TransferListenerBase* listener, Mode mode);
        void remove(TransferListenerBase* listener);
        bool exists(DataTypeID dtid) const;
        void cleanup(MonotonicTime ts);
        void handleFrame(const RxFrame& frame);

        int getNumEntries() const { return list_.getLength(); }
    };

    ListenerRegistry lmsg_;
    ListenerRegistry lsrv_req_;
    ListenerRegistry lsrv_resp_;

    LoopbackFrameListenerRegistry loopback_listeners_;

    NodeID self_node_id_;

    void handleFrame(const CanRxFrame& can_frame);
    void handleLoopbackFrame(const CanRxFrame& can_frame);

public:
    Dispatcher(ICanDriver& driver, IPoolAllocator& allocator, ISystemClock& sysclock, IOutgoingTransferRegistry& otr)
        : canio_(driver, allocator, sysclock)
        , sysclock_(sysclock)
        , outgoing_transfer_reg_(otr)
    { }

    int spin(MonotonicTime deadline);

    /**
     * Refer to CanIOManager::send() for the parameter description
     */
    int send(const Frame& frame, MonotonicTime tx_deadline, MonotonicTime blocking_deadline, CanTxQueue::Qos qos,
             CanIOFlags flags, uint8_t iface_mask);

    void cleanup(MonotonicTime ts);

    bool registerMessageListener(TransferListenerBase* listener);
    bool registerServiceRequestListener(TransferListenerBase* listener);
    bool registerServiceResponseListener(TransferListenerBase* listener);

    void unregisterMessageListener(TransferListenerBase* listener);
    void unregisterServiceRequestListener(TransferListenerBase* listener);
    void unregisterServiceResponseListener(TransferListenerBase* listener);

    bool hasSubscriber(DataTypeID dtid) const;
    bool hasPublisher(DataTypeID dtid) const;
    bool hasServer(DataTypeID dtid) const;

    int getNumMessageListeners()         const { return lmsg_.getNumEntries(); }
    int getNumServiceRequestListeners()  const { return lsrv_req_.getNumEntries(); }
    int getNumServiceResponseListeners() const { return lsrv_resp_.getNumEntries(); }

    IOutgoingTransferRegistry& getOutgoingTransferRegistry() { return outgoing_transfer_reg_; }

    LoopbackFrameListenerRegistry& getLoopbackFrameListenerRegistry() { return loopback_listeners_; }

    NodeID getNodeID() const { return self_node_id_; }
    bool setNodeID(NodeID nid);

    bool isPassiveMode() const { return !getNodeID().isUnicast(); }

    const ISystemClock& getSystemClock() const { return sysclock_; }
    ISystemClock& getSystemClock() { return sysclock_; }

    const CanIOManager& getCanIOManager() const { return canio_; }

    const TransferPerfCounter& getTransferPerfCounter() const { return perf_; }
    TransferPerfCounter& getTransferPerfCounter() { return perf_; }
};

}
