
#include "RSVP_MPLS.h"
#include "RSVP_Log.h"
#include "RSVP_OIatPSB.h"
#include "RSVP_OutISB.h"
#include "RSVP_PSB.h"
#include "RSVP_PHopSB.h"
#include "RSVP_Session.h"
#include "RSVP_Message.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_Global.h"
#include "SwitchCtrl_Global.h"
//#include "SNMP_Session.h"
//#include "CLI_Session.h"
#if MPLS_REAL
#include "RSVP_RoutingService.h"
#include <linux/mpls.h>
#include <sys/ioctl.h>
#endif

#if defined(MPLS_WISCONSIN)
#include <asm/types.h>
#include <linux/rtnetlink.h>

extern "C" {
int rtnl_open();
int rtnl_recvfrom(int fd);
int send_nhlfe(int netlink,struct mpls_out_label_req *mol_req,int cmd);
int send_ilm(int netlink,struct mpls_in_label_req *mil_req,int cmd);
int send_ftn(int netlink,struct mpls_bind_fec_req *mbf_req,int cmd);
int send_xc(int netlink,struct mpls_xconnect_req *mx_req,int cmd);
int send_instr(int netlink,struct mpls_instruction_req *mir_req,int cmd);
}

#elif defined(MPLS_CAMBRIDGE)
#include <linux/mpls.h>
#include <net/if_arp.h>                           // struct arpreq

extern "C" {
int mpls_init(void);
void mpls_cleanup(void);
int mpls_add_switch_mapping(switch_mapping_t *sm);
int mpls_del_switch_mapping(cid_t *cid);
int mpls_add_port_mapping(port_mapping_t *pm);
int mpls_del_port_mapping(int port);
int mpls_add_ingress_mapping(ingress_mapping_t *im);
int mpls_del_ingress_mapping(fec_t *fec);
int mpls_add_egress_mapping(egress_mapping_t *em);
int mpls_del_egress_mapping(cid_t *cid);
int mpls_flush_all(void);
int mpls_debug_on(void); 
int mpls_debug_off(void);
}
#endif

const uint32 MPLS::filterHashSize = 1024;
const uint32 MPLS::minLabel = 16;
const uint32 MPLS::maxLabel = (1 << 20) - 1;

MPLS::MPLS( uint32 num, uint32 begin, uint32 end )
	: labelSpaceNum(num), labelSpaceBegin(begin), labelSpaceEnd(end), currentLabel(0),
	numberOfAllocatedLabels(0), labelHash(NULL),
	ingressClassifiers(filterHashSize) {

	if ( labelSpaceBegin < minLabel ) labelSpaceBegin = minLabel;
	if ( !labelSpaceEnd) labelSpaceEnd = maxLabel;
	currentLabel = labelSpaceBegin - 1;
}

bool MPLS::init() {
	labelHash = new uint32[RSVP_Global::labelHashCount];
	initMemoryWithZero( labelHash, sizeof(uint32)*RSVP_Global::labelHashCount );
#if defined(MPLS_WISCONSIN)
	netlink = CHECK( rtnl_open() );
	int fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	uint32 i;
	for ( i = 0; i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
		const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByLIH(i);
		if ( lif && !lif->isDisabled() && lif->hasEnabledMPLS() && lif->getSysIndex() != -1 ) {
			static struct mpls_labelspace_req mls_req;
			initMemoryWithZero( &mls_req, sizeof(mls_req) );
			mls_req.mls_ifindex = lif->getSysIndex();
			mls_req.mls_labelspace = labelSpaceNum;
			CHECK( ioctl( fd, SIOCSLABELSPACEMPLS, &mls_req ) );
		}
	}
	CHECK( close( fd ) );
#elif defined(MPLS_CAMBRIDGE)
	CHECK( mpls_init() );
	static port_mapping_t pm;
	initMemoryWithZero( &pm, sizeof(pm) );
	pm.type = LOCAL_PORT;
	pm.id = 0;
	CHECK( mpls_add_port_mapping( &pm ) );
#endif
	return true;
}

MPLS::~MPLS() {

#if MPLS_REAL
	uint32 x = 0;
	for ( ; x < filterHashSize; ++x ) {
		SortableHash<MPLS_Classifier*>::HashBucket::ConstIterator iter = ingressClassifiers[x].begin();
		for ( ; iter != ingressClassifiers[x].end(); ++iter ) {
			LOG(2)( Log::MPLS, "MPLS: cleaning up routing entry to", (*iter)->destAddress );
			RSVP_Global::rsvp->getRoutingService().delUnicastRoute( (*iter)->destAddress, NULL, NetAddress(0) );
			delete (*iter);
		}
	}
#endif

#if defined(MPLS_WISCONSIN)
	int fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	uint32 i;
	for ( i = 0; i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
		const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByLIH(i);
		if ( lif && !lif->isDisabled() && lif->hasEnabledMPLS() && lif->getSysIndex() != -1 ) {
			static struct mpls_labelspace_req mls_req;
			initMemoryWithZero( &mls_req, sizeof(mls_req) );
			mls_req.mls_ifindex = lif->getSysIndex();
			mls_req.mls_labelspace = -1;
			CHECK( ioctl( fd, SIOCSLABELSPACEMPLS, &mls_req ) );
		}
	}
	close( fd );
	close( netlink );
#elif defined(MPLS_CAMBRIDGE)
	mpls_del_port_mapping( 0 );
	mpls_flush_all();
	mpls_cleanup();
#endif

	if (labelHash) delete [] labelHash;
}

inline uint32 MPLS::allocateInLabel() {
	if ( numberOfAllocatedLabels >= RSVP_Global::labelHashCount ) {
		FATAL(4)( Log::Fatal, "FATAL ERROR: number of labels", numberOfAllocatedLabels, "has exceeded the hash container size", RSVP_Global::labelHashCount );
		abortProcess();
	}
	do {
		currentLabel += 1;
		if ( currentLabel > labelSpaceEnd ) currentLabel = labelSpaceBegin;
	} while ( labelHash[currentLabel%RSVP_Global::labelHashCount] != 0 );
	LOG(2)( Log::MPLS, "MPLS: allocated label", currentLabel );
	numberOfAllocatedLabels += 1;
	return currentLabel;
	//return (uint32)1089538;  //port 1-1-10-2, just for current test, since we don't have any LMP nor do we have LabelSet from Movaz RE!
}

inline void MPLS::freeInLabel( uint32 label ) {
	LOG(2)( Log::MPLS, "MPLS: freeing label", label );
	numberOfAllocatedLabels -= 1;
	if (numberOfAllocatedLabels >= RSVP_Global::labelHashCount)
		numberOfAllocatedLabels = 0; 
	labelHash[label%RSVP_Global::labelHashCount] = 0;
}

inline MPLS_Classifier* MPLS::internCreateClassifier( const SESSION_Object& session, const SENDER_Object& sender, uint32 handle ) {
	LOG(3)( Log::MPLS, "MPLS: setting filter for", session, sender );
	MPLS_Classifier* filter = new MPLS_Classifier( session.getDestAddress() );
	SortableHash<MPLS_Classifier*>::HashBucket::Iterator iter = ingressClassifiers.lower_bound( filter ); 
	if ( iter != ingressClassifiers.getHashBucket( filter ).end() && **iter == *filter ) {
		delete filter;
		(*iter)->refCount += 1;
	} else {
		LOG(3)( Log::MPLS, "MPLS: creating filter for", session, sender );
		iter = ingressClassifiers.insert( iter, filter );
#if MPLS_REAL
		LOG(2)( Log::MPLS, "MPLS: creating routing entry to", (*iter)->destAddress );
		RSVP_Global::rsvp->getRoutingService().addUnicastRoute( session.getDestAddress(), NULL, NetAddress(0), handle );
#endif
	}
	return *iter;
}

inline void MPLS::internDeleteClassifier( const MPLS_Classifier* f ) {
	const_cast<MPLS_Classifier*>(f)->refCount -= 1;
	if ( f->refCount == 0 ) {
#if MPLS_REAL
		LOG(2)( Log::MPLS, "MPLS: removing routing entry to", f->destAddress );
		RSVP_Global::rsvp->getRoutingService().delUnicastRoute( f->destAddress, NULL, NetAddress(0) );
#endif
		LOG(2)( Log::MPLS, "MPLS: removing filter to", f->destAddress );
		ingressClassifiers.erase_key( const_cast<MPLS_Classifier*>(f) );
		delete f;
	}
}

void MPLS::handleOutLabel( OIatPSB& oiatpsb, uint32 label, const Hop& nhop ) {
	LOG(4)( Log::MPLS, "MPLS: storing output label", label, "for", oiatpsb );
	MPLS_OutLabel* outLabel = new MPLS_OutLabel( label );
#if defined(MPLS_WISCONSIN)
	outLabel->handle = nhop.getLogicalInterface().getSysIndex();
	static struct mpls_out_label_req mol_req;
	initMemoryWithZero( &mol_req, sizeof(mol_req) );
	mol_req.mol_label.ml_type = MPLS_LABEL_GEN;
	mol_req.mol_label.u.ml_gen = outLabel->getLabel();
	mol_req.mol_label.ml_index = outLabel->getLifSysIndex();
	reinterpret_cast<sockaddr_in&>(mol_req.mol_nh).sin_family = AF_INET;
	reinterpret_cast<sockaddr_in&>(mol_req.mol_nh).sin_addr.s_addr = nhop.getAddress().rawAddress();
	CHECK( send_nhlfe( netlink, &mol_req, RTM_NEWNHLFE ) );
#elif defined(MPLS_CAMBRIDGE)
	outLabel->handle = nhop.getHopInfoMPLS();
#endif
	outLabel->setLabelCType(oiatpsb.getRequestedOutLabelType());
	oiatpsb.setOutLabel( outLabel );
}

const MPLS_InLabel* MPLS::setInLabel( PSB& psb ) {

	MPLS_InLabel* inLabel;
	if (psb.getLABEL_SET_Object())
		inLabel = new MPLS_InLabel((const_cast<LABEL_SET_Object*>(psb.getLABEL_SET_Object()))->getASubChannel());
	else if (psb.hasSUGGESTED_LABEL_Object())
		inLabel = new MPLS_InLabel(psb.getSUGGESTED_LABEL_Object().getLabel());
	else if (psb.hasUPSTREAM_OUT_LABEL_Object()) //Make the best guess, just set the return label to be the same as the upstream label
		inLabel = new MPLS_InLabel(psb.getUPSTREAM_OUT_LABEL_Object().getLabel());
	else
		inLabel = new MPLS_InLabel( allocateInLabel() );
	inLabel->setLabelCType(psb.getLABEL_REQUEST_Object().getRequestedLabelType());
		
#if defined(MPLS_WISCONSIN)
	static struct mpls_in_label_req mil_req;
	initMemoryWithZero( &mil_req, sizeof(mil_req) );
	mil_req.mil_label.ml_type = MPLS_LABEL_GEN;
	mil_req.mil_label.u.ml_gen = inLabel->getLabel();
	mil_req.mil_label.ml_index = labelSpaceNum;
	CHECK( send_ilm( netlink, &mil_req, RTM_NEWILM ) );
#elif defined(MPLS_CAMBRIDGE)
	inLabel->handle = psb.getPHopSB().getHop().getHopInfoMPLS();
#endif
	return inLabel;
}

bool MPLS::bindInAndOut( PSB& psb, const MPLS_InLabel& il, const MPLS_OutLabel& ol, const MPLS* inLabelSpace ) {
	if ( !inLabelSpace ) inLabelSpace = this;
	LOG(6)( Log::MPLS, "MPLS: binding outgoing label", ol.getLabel(), "to input label", il.getLabel(), "from label space", inLabelSpace->labelSpaceNum );
#if defined(MPLS_WISCONSIN)
	static struct mpls_xconnect_req mx_req;
	initMemoryWithZero( &mx_req, sizeof(mx_req) );
	mx_req.mx_in.ml_type = MPLS_LABEL_GEN;
	mx_req.mx_in.u.ml_gen = il.getLabel();
	mx_req.mx_in.ml_index = inLabelSpace->labelSpaceNum;
	mx_req.mx_out.ml_type = MPLS_LABEL_GEN;
	mx_req.mx_out.u.ml_gen = ol.getLabel();
	mx_req.mx_out.ml_index = ol.getLifSysIndex();
	CHECK( send_xc( netlink, &mx_req, RTM_NEWXC ) );
#elif defined(MPLS_CAMBRIDGE)
	static switch_mapping_t sm;
	initMemoryWithZero( &sm, sizeof(sm) );
	sm.in_cid.port = il.getPort();
	sm.in_cid.label = il.getLabel();
	sm.out_cid.port = ol.getPort();
	sm.out_cid.label = ol.getLabel();
	CHECK( mpls_add_switch_mapping( &sm ) );
#endif
	if (!psb.getVLSR_Route().empty()){
		VLSRRoute::ConstIterator iter = psb.getVLSR_Route().begin();
		for ( ; iter != psb.getVLSR_Route().end(); ++iter ) {
			NetAddress ethSw = (*iter).switchID; // ethSw is the physical switch address only for non-subnet sessions
			SwitchCtrlSessionList::Iterator sessionIter = RSVP_Global::switchController->getSessionList().begin();
			bool noError = false;
			for (; sessionIter != RSVP_Global::switchController->getSessionList().end(); ++sessionIter ) {

				 //@@@@ >>Xi2007<<
				if ( (*sessionIter)->getSessionName().leftequal("subnet-uni") ) {
                                if ((*iter).switchID.rawAddress() == ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getPseudoSwitchID()) {
					SimpleList<uint8> ts_list;
					//UNI session error will fail the RSVP session
					switch (((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getUniState()) {
					case Message::PathErr:
					case Message::PathTear:
					case Message::ResvErr:
					case Message::ResvTear:
						RSVP_Global::messageProcessor->sendResvErrMessage( 0, ERROR_SPEC_Object::Notify, ERROR_SPEC_Object::SubnetUNISessionFailed ); //UNI ERROR ??
						LOG(2)( Log::MPLS, "VLSR: SubnetUNI session failed with message state : ", ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getUniState() );
						return false;
						break;
					case Message::InitAPI: // inital state for TL1_TELNET only
						if (CLI_SESSION_TYPE == CLI_TL1_TELNET) {

							//verify
							if (((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->hasSourceDestPortConflict()) {
								LOG(2)( Log::MPLS, "VLSR-Subnet Control: hasSourceDestPortConflict() == True: cannot crossconnect from to to the same ETTP on ", (*sessionIter)->getSwitchInetAddr());
								return false;
							}
                                                    
							//connect
							if (!(*sessionIter)->connectSwitch()) {
								LOG(2)( Log::MPLS, "VLSR-Subnet Connect: Cannot connect to switch via TL1_TELNET: ", (*sessionIter)->getSwitchInetAddr());
								(*sessionIter)->disconnectSwitch();
								return false;
							}

                            if ( ((*iter).inPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_SRC ||((*iter).outPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_DEST ) {
                                // $$$$ verifying instead of sync'ing timeslots (for inconsistency, check error messages in log)
                                //if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->syncTimeslotsMap() ) {
                                if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->verifyTimeslotsMap() ) {
                                    (*sessionIter)->disconnectSwitch();
                                    return false;
                                }
                                
                                //create VCG for LOCAL_ID_TYPE_SUBNET_UNI_SRC OR LOCAL_ID_TYPE_SUBNET_UNI_DEST
                                if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->createVCG((*iter).vlanTag) ) {
                                    (*sessionIter)->disconnectSwitch();
                                    return false;
                                }

                                if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceClient() 
                                    && ((*iter).inPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_SRC) {
                                    //create source GTP 
                                    if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->createGTP() ) {
                                        (*sessionIter)->disconnectSwitch();
                                        return false;
                                    }
                                    if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceDestSame() ) {
                                        //create CRS for Source == Destination
                                        if( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->createCRS() ) {
                                            (*sessionIter)->disconnectSwitch();
                                            return false;
                                        }
                                    }
                                    else {
                                        //create SNC
                                        if( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->createSNC() ) {
                                            (*sessionIter)->disconnectSwitch();
                                            return false;
                                        }
                                    }
                                }
                            }
                            else if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceClient() 
                                && ((*iter).inPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_DEST
                                && ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceDestSame() )
                            {
                                //create destination GTP (only needed for local XConn)
                                if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->createGTP() ) {
                                    (*sessionIter)->disconnectSwitch();
                                    return false;
                                }
                            }

                            //disconnect
                            (*sessionIter)->disconnectSwitch();

						}
						// no break; continue to next case clauses !

					case Message::Resv:
					case Message::ResvConf:
		                            if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceClient() && ((*iter).inPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_SRC) {
							// Update ingress link bandwidth
		                                   RSVP_Global::rsvp->getRoutingService().holdBandwidthbyOSPF((*iter).inPort, (*iter).bandwidth, true); //true == decrease
		                                   // Update time slots
							((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getTimeslots(ts_list);
							if (ts_list.size() > 0)
								RSVP_Global::rsvp->getRoutingService().holdTimeslotsbyOSPF((*iter).inPort, ts_list, true);
						}
						if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceClient() && ((*iter).outPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_DEST ) {
							// Update egress link bandwidth
		                                   RSVP_Global::rsvp->getRoutingService().holdBandwidthbyOSPF((*iter).outPort, (*iter).bandwidth, true); //true == decrease
		                                   // Update time slots
							((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getTimeslots(ts_list);
							if (ts_list.size() > 0)
								RSVP_Global::rsvp->getRoutingService().holdTimeslotsbyOSPF((*iter).outPort, ts_list, true);
						}
						noError = true;
						break;
					default:
						noError = true;
						break;
					}
                                }

				    continue;
 				} 

				if ((*sessionIter)->getSwitchInetAddr()==ethSw && (*sessionIter)->isValidSession()){
					noError = true;
					uint32 vlan;
                                   if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL || ((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                    {
                                      vlan = (*iter).vlanTag;
                                    }
                                   //else if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_GROUP || ((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP)
                                   else if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP)
                                    {
                                      vlan =   (*iter).inPort & 0xffff;
                                    }
                                   //else if (((*iter).outPort >> 16) == LOCAL_ID_TYPE_GROUP || ((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP)
                                   else if (((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP)
								   	{
                                      vlan =   (*iter).outPort & 0xffff;
                                    }
                                   else
                                        vlan = (*sessionIter)->findEmptyVLAN();

                                   if (!(*sessionIter)->verifyVLAN(vlan))
                                    {
        					LOG(5)( Log::MPLS, "VLSR: Cannot verify VLAN Tag(ID). ", vlan, " on Switch: ", ethSw, 
                                                        " >>> Creating a new VLAN...");
                                          if (!(*sessionIter)->createVLAN(vlan)) {
                					LOG(5)( Log::MPLS, "VLSR: Creating a new VLAN ID:", vlan, " on Switch: ", ethSw, " has failed!" );
                					noError = false;
                                          }
                                    }

					//if (vlan){
					if (noError){
        					PortList portList;
                                          uint32 inPort = (*iter).inPort;
                                          uint32 taggedPorts = 0;
                                          if ((inPort >> 16) == LOCAL_ID_TYPE_NONE)
                                              portList.push_back(inPort);
                                          else if ((inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                              portList.push_back(inPort & 0xffff);
                                          else {
                                              DRAGON_UNI_Object* uni = (DRAGON_UNI_Object*)psb.getDRAGON_UNI_Object();
                                              if (uni && uni->getSrcTNA().local_id == UNI_AUTO_TAGGED_LCLID)
                                                  inPort = RSVP_Global::rsvp->getLocalIdByIfName((char*)uni->getIngressCtrlChannel().name);
                                              SwitchCtrl_Global::getPortsByLocalId(portList, inPort);
                                          }
                                          if (inPort == ((LOCAL_ID_TYPE_TAGGED_GROUP << 16) | 0)) //NULL local-ID
                                          {
                                              portList.clear();
                                          }
                                          else if (portList.size() == 0){
                                                LOG(2)( Log::MPLS, "VLSR: Unrecognized port/localID at ingress: ", inPort);
                                                //return false;
                                                noError = false;
                                          }
                                          while (portList.size()) {
                                                 uint32 port = portList.front(); //reuse the variable port
        						LOG(4)(Log::MPLS, "VLSR: Moving ingress port#",  GetSwitchPortString(port), " to VLAN #", vlan);
                                                 if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP || ((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL) {
                                                        (*sessionIter)->movePortToVLANAsTagged(port, vlan);
                                                 	//Up to 32 ports supported. Only default RFC2674 switch switch (e.g. Dell, Intel) use this.
                                                        taggedPorts |= (1<<(32-port));
                                                    }
                                                 else
                                                    (*sessionIter)->movePortToVLANAsUntagged(port, vlan);

                                                 LOG(4)(Log::MPLS, "VLSR: Perform bidirectional bandwidth policing and limitation on port#",  GetSwitchPortString(port), "for VLAN #", vlan);
							//Perform rate policing and limitation on the port, which is both input and output port
							//as the VLAN is duplex.
                                                (*sessionIter)->policeInputBandwidth(true, port, vlan, (*iter).bandwidth);
                                                 (*sessionIter)->limitOutputBandwidth(true, port, vlan,  (*iter).bandwidth); //$$$$ To be moved into bindUpstreamInAndOut

							//deduct bandwidth from the link associated with the port (revserse link bandwidth for bidirectional LSP only)
							//$$$$ To be moved into bindUpstreamInAndOut
							RSVP_Global::rsvp->getRoutingService().holdBandwidthbyOSPF(port, (*iter).bandwidth, true); //true == deduct
                                                portList.pop_front();
                                          }
                                          portList.clear();
                                          //taggedPorts = 0;
                                          uint32 outPort = (*iter).outPort;
                                          if ((outPort >> 16) == LOCAL_ID_TYPE_NONE)
                                              portList.push_back(outPort);
                                          else if ((outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                              portList.push_back(outPort & 0xffff);
                                          else {
                                              DRAGON_UNI_Object* uni = (DRAGON_UNI_Object*)psb.getDRAGON_UNI_Object();
                                              if (uni && uni->getDestTNA().local_id == UNI_AUTO_TAGGED_LCLID)
                                                  outPort = RSVP_Global::rsvp->getLocalIdByIfName((char*)uni->getEgressCtrlChannel().name);
                                              SwitchCtrl_Global::getPortsByLocalId(portList, outPort);
                                          }
                                          if (outPort == ((LOCAL_ID_TYPE_TAGGED_GROUP << 16) | 0)) //NULL local-ID
                                          {
                                              portList.clear();
                                          }
                                          else if (portList.size() == 0){
        					      LOG(2)( Log::MPLS, "VLSR: Unrecognized port/localID at egress: ", outPort);
                                                //return false;
                                                noError = false;
                                          }
                                          while (portList.size()) {
                                                 uint32 port = portList.front();
                                                 LOG(4)(Log::MPLS, "VLSR: Moving egress port#",  GetSwitchPortString(port), " to VLAN #", vlan);
                                                 if (((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP || ((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL) {
                                                        (*sessionIter)->movePortToVLANAsTagged(port, vlan);
                                                 	//Up to 32 ports supported. Only default RFC2674 switch switch (e.g. Dell, Intel) use this.
                                                        taggedPorts |= (1<<(32-port));
                                                    }
                                                 else
                                                        (*sessionIter)->movePortToVLANAsUntagged(port, vlan);

                                                 LOG(4)(Log::MPLS, "VLSR: Perform bidirectional bandwidth policing and limitation on port#",  GetSwitchPortString(port), "for VLAN #", vlan);
							//Perform rate policing and limitation on the port, which is both input and output port
							//as the VLAN is duplex.
                                                (*sessionIter)->policeInputBandwidth(true, port, vlan, (*iter).bandwidth);  //$$$$ To be moved into bindUpstreamInAndOut
                                                 (*sessionIter)->limitOutputBandwidth(true, port, vlan,  (*iter).bandwidth);

							//deduct bandwidth from the link associated with the port
							RSVP_Global::rsvp->getRoutingService().holdBandwidthbyOSPF(port, (*iter).bandwidth, true); //true == deduct
                                                portList.pop_front();
                                          }
										  
                                          if (taggedPorts != 0)
                                            {
                                                 //Set vlan ports to be "tagged" 
                                                 //Only default RFC2674 switch switch (e.g. Dell, Intel) does something; Others simply return true.
	                                  		(*sessionIter)->setVLANPortsTagged(taggedPorts, vlan);
							//remove the VTAG that is taken by the LSP
							LOG(4)(Log::MPLS, "VLSR: Set tagged ports:",  taggedPorts, " in VLAN #", vlan);
							if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)   //$$$$ To be moved into bindUpstreamInAndOut
								RSVP_Global::rsvp->getRoutingService().holdVtagbyOSPF((*iter).inPort & 0xffff, (*iter).vlanTag, true); //false == hold
							if (((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
								RSVP_Global::rsvp->getRoutingService().holdVtagbyOSPF((*iter).outPort & 0xffff, (*iter).vlanTag, true); //false == hold
                                          }
					}
					else{
						noError = false;
						LOG(2)( Log::MPLS, "VLSR: Cannot find an empty VLAN on switch : ", ethSw);
					}
					break;
				}
			}
			if (!noError){
				return false;
			}
		}
	}

	return true;

}

bool MPLS::refreshVLSRbyLocalId( PSB& psb, uint32 lclid) {
	if (!psb.getVLSR_Route().empty()) {
		VLSRRoute::ConstIterator iter = psb.getVLSR_Route().begin();
		for ( ; iter != psb.getVLSR_Route().end(); ++iter ) {
			NetAddress ethSw = (*iter).switchID;
			SwitchCtrlSessionList::Iterator sessionIter = RSVP_Global::switchController->getSessionList().begin();
			for (; sessionIter != RSVP_Global::switchController->getSessionList().end(); ++sessionIter) {
				if ((*sessionIter)->getSwitchInetAddr()==ethSw && (*sessionIter)->isValidSession()) {
					uint32 vlan;
                                   if ((*iter).inPort == lclid)
                                    {
                                      vlan = (*iter).vlanTag;
                                      (*sessionIter)->adjustVLANbyLocalId(vlan, lclid, ((*iter).outPort & 0xffff));
                                    }
                                   if ((*iter).outPort == lclid)
                                    {
                                      vlan = (*iter).vlanTag;
                                      (*sessionIter)->adjustVLANbyLocalId(vlan, lclid, ((*iter).inPort & 0xffff));
                                    }
				}
			}
		}
	}

	return true;
}

void MPLS::createIngressClassifier( const SESSION_Object& session, const SENDER_Object& sender, const MPLS_OutLabel& ol ) {
	LOG(5)( Log::MPLS, "MPLS: creating ingress binding for", session, sender, "to label", ol.getLabel() );
	// create filter (i.e. routing entry)
	const_cast<MPLS_OutLabel&>(ol).filter = internCreateClassifier( session, sender, session.getDestAddress().rawAddress() );
#if defined(MPLS_WISCONSIN)
	// bind FEC to label
	static struct mpls_bind_fec_req mbf_req;
	initMemoryWithZero( &mbf_req, sizeof(mbf_req) );
	mbf_req.mbf_fec.prefix = session.getDestAddress().rawAddress();
	mbf_req.mbf_fec.len = 32;
	mbf_req.mbf_label.ml_type = MPLS_LABEL_GEN;
	mbf_req.mbf_label.u.ml_gen = ol.getLabel();
	mbf_req.mbf_label.ml_index = ol.getLifSysIndex();
	CHECK( send_ftn( netlink, &mbf_req, RTM_NEWFTN ) );
#elif defined(MPLS_CAMBRIDGE)
	static ingress_mapping_t im;
	initMemoryWithZero( &im, sizeof(im) );
	im.fec.proto = MPLSPROTO_IPV4;
	im.fec.u.ipv4.tclassid = session.getDestAddress().rawAddress();
	im.in_cid.port = 0;
	im.in_cid.label = session.getDestAddress().rawAddress();
	CHECK( mpls_add_ingress_mapping( &im ) );
	static switch_mapping_t sm;
	initMemoryWithZero( &sm, sizeof(sm) );
	sm.in_cid.port = 0;
	sm.in_cid.label = session.getDestAddress().rawAddress();
	sm.out_cid.port = ol.getPort();
	sm.out_cid.label = ol.getLabel();
	CHECK( mpls_add_switch_mapping( &sm ) );
#endif
}

void MPLS::createEgressBinding( const MPLS_InLabel& il, const LogicalInterface& lif ) {
	LOG(4)( Log::MPLS, "MPLS: creating egress binding for label", il.getLabel(), "to", lif.getName() );
#if defined(MPLS_CAMBRIDGE)
	static egress_mapping_t em;
	initMemoryWithZero( &em, sizeof(em) );
	em.in_cid.port = il.getPort();
	em.in_cid.label = il.getLabel();
	em.egress.proto = MPLSPROTO_IPV4;
	strcpy( em.egress.u.ipv4.ifname, lif.getName().chars() );
	CHECK( mpls_add_egress_mapping(&em) );
#endif
	const_cast<MPLS_InLabel&>(il).egressBinding = true;
}

bool MPLS::bindUpstreamInAndOut( PSB& psb) {
	LOG(4)( Log::MPLS, "MPLS: binding upstream outgoing label", psb.getUPSTREAM_OUT_LABEL_Object().getLabel(), 
					  "to input label", psb.getUPSTREAM_IN_LABEL_Object().getLabel());
	return true;
}

bool MPLS::createUpstreamIngressClassifier( const SESSION_Object& session, PSB& psb) {
	LOG(4)( Log::MPLS, "MPLS: creating upstream ingress binding for", session, "to input label", psb.getUPSTREAM_IN_LABEL_Object().getLabel());
	return true;
}

bool MPLS::createUpstreamEgressBinding( PSB& psb, const LogicalInterface& lif) {
	LOG(4)( Log::MPLS, "MPLS: creating upstream egress binding for label", psb.getUPSTREAM_OUT_LABEL_Object().getLabel(), "to", lif.getName());
	return true;
}

void MPLS::deleteInLabel(PSB& psb, const MPLS_InLabel* il ) {
	LOG(2)( Log::MPLS, "MPLS: deleting input label", il->getLabel() );
#if defined(MPLS_WISCONSIN)
	static struct mpls_in_label_req mil_req;
	initMemoryWithZero( &mil_req, sizeof(mil_req) );
	mil_req.mil_label.ml_type = MPLS_LABEL_GEN;
	mil_req.mil_label.u.ml_gen = il->getLabel();
	mil_req.mil_label.ml_index = labelSpaceNum;
	CHECK( send_ilm( netlink, &mil_req, RTM_DELILM ) );
#elif defined(MPLS_CAMBRIDGE)
	static cid_t cid;
	initMemoryWithZero( &cid, sizeof(cid) );
	cid.port = il->getPort();
	cid.label = il->getLabel();
	if ( il->egressBinding ) {
		CHECK( mpls_del_egress_mapping(&cid) );
	} else {
		CHECK( mpls_del_switch_mapping(&cid) );
	}
#endif
	freeInLabel( il->getLabel() );
	delete il;

	if (!psb.getVLSR_Route().empty()){
		VLSRRoute::ConstIterator iter = psb.getVLSR_Route().begin();
		for ( ; iter != psb.getVLSR_Route().end(); ++iter ) {
			NetAddress ethSw = (*iter).switchID;
			SwitchCtrlSessionList::Iterator sessionIter = (--RSVP_Global::switchController->getSessionList().end());
			for (; sessionIter != RSVP_Global::switchController->getSessionList().end(); --sessionIter ) {

				 //@@@@ >>Xi2007<<
				if ( (*sessionIter)->getSessionName().leftequal("subnet-uni") ){
					SimpleList<uint8> ts_list;
					if( (*iter).switchID.rawAddress() == ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getPseudoSwitchID()) {

						if (CLI_SESSION_TYPE == CLI_TL1_TELNET) {
							bool noErr = true;

							//verify
							if (((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->hasSourceDestPortConflict()) {
								LOG(2)( Log::MPLS, "VLSR-Subnet Control: hasSourceDestPortConflict() == True: cannot crossconnect from to to the same ETTP on ", (*sessionIter)->getSwitchInetAddr());
								return;
							}

							//connect
							if (!(*sessionIter)->connectSwitch()){
								LOG(2)( Log::MPLS, "VLSR-Subnet Connect: Cannot connect to switch via TL1_TELNET: ", (*sessionIter)->getSwitchInetAddr());
								return;
							}

						    if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceClient() && ((*iter).inPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_SRC ) {

						        if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceDestSame() ) {
						            //delete CRS for Source == Destination
						            if( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->hasCRS() )
						                noErr = noErr && ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->deleteCRS();
						        }
						        else {
						            //delete SNC
						            if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->hasSNC() )
						                noErr = noErr && ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->deleteSNC();
						        }
						            
						        //delete GTP (for SNC: source only; for CRS: both source and dest interfaces)
						        if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->hasGTP())
						            noErr = noErr && ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->deleteGTP();

								//delete VCG for LOCAL_ID_TYPE_SUBNET_UNI_SRC
								if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->hasVCG())
							        noErr == noErr && ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->deleteVCG();

							    (*sessionIter)->disconnectSwitch();
						    }
						    else if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceClient() && ((*iter).outPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_DEST) {
								if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->hasVCG()) {
								    //$$$$ Special handling to adjust the sequence of SNC-VCG-deletion at destination node.
									if (((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->hasSystemSNCHolindgCurrentVCG()) {
										pid_t pid;
										switch( pid=fork() )
										{
										case 0: // child process for delayed waiting-and-deleting procedure
	 										if (((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->waitUntilSystemSNCDisapear())
											{
												((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->deleteVCG(); 
											}
											(*sessionIter)->disconnectSwitch();
											exit(0);
											break;
										case -1: // error
											LOG(1)( Log::MPLS, "VLSR-Subnet Fatal Error: cannot fork a child process for the waiting-and-deleting procedure!");
											exit(-1);
											break;
										default: // parent (orininal) process back to main logic loop
											((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->closePipe();
											continue;
										}
									}
									else
									{
										((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->deleteVCG();
									    (*sessionIter)->disconnectSwitch();
									}
								}
							}

						    if (!noErr) return; // otherwise, continue to update bandwidth and timeslots.
						}

						if ( ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceClient() && ((*iter).inPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_SRC ) {

						    if (((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getUniState() != Message::InitAPI)
		   					((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->releaseRsvpPath();

                                  // Update ingress link bandwidth
                                  RSVP_Global::rsvp->getRoutingService().holdBandwidthbyOSPF((*iter).inPort, (*iter).bandwidth, false); //false == increase
                                  // Update time slots
                                  ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getTimeslots(ts_list);
                                  if (ts_list.size() > 0)
                                      RSVP_Global::rsvp->getRoutingService().holdTimeslotsbyOSPF((*iter).inPort, ts_list, false);
						}
						if ( !((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->isSourceClient() && ((*iter).outPort >> 16) == LOCAL_ID_TYPE_SUBNET_UNI_DEST ) {

                                  // Update egress link bandwidth
                                  RSVP_Global::rsvp->getRoutingService().holdBandwidthbyOSPF((*iter).outPort, (*iter).bandwidth, false); //false == increase
                                  // Update time slots
                                  ((SwitchCtrl_Session_SubnetUNI*)(*sessionIter))->getTimeslots(ts_list);
                                  if (ts_list.size() > 0)
                                      RSVP_Global::rsvp->getRoutingService().holdTimeslotsbyOSPF((*iter).outPort, ts_list, false);
						}
					}

					continue;
				}

				if ((*sessionIter)->getSwitchInetAddr()==ethSw && (*sessionIter)->isValidSession()){
                                      PortList portList;
                                      uint32 vlanID;
                                      uint32 inPort = (*iter).inPort;

                                      if ((inPort >> 16) == LOCAL_ID_TYPE_NONE)
                                         portList.push_back(inPort);
                                      else if ((inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                          portList.push_back(inPort & 0xffff);
                                      else
                                      {
                                          DRAGON_UNI_Object* uni = (DRAGON_UNI_Object*)psb.getDRAGON_UNI_Object();
                                          if (uni && uni->getSrcTNA().local_id == UNI_AUTO_TAGGED_LCLID)
                                              inPort = RSVP_Global::rsvp->getLocalIdByIfName((char*)uni->getIngressCtrlChannel().name);
                                          SwitchCtrl_Global::getPortsByLocalId(portList, inPort);
                                      }
                                      if (inPort == ((LOCAL_ID_TYPE_TAGGED_GROUP << 16) | 0)) //NULL local-ID
                                      {
                                          portList.clear();
                                      }
                                      else if (portList.size() == 0){
    					       LOG(2)( Log::MPLS, "VLSR: Unrecognized port/localID at ingress: ", inPort);
                                          //continue;
                                      }
                                      while (portList.size()) {
                                            uint32 port = portList.front();
                                            vlanID = (*iter).vlanTag;
                                            if (vlanID ==0)
                                                vlanID = (*sessionIter)->getActiveVlanId(port);
                                            if (vlanID != 0) {
                                                (*sessionIter)->removePortFromVLAN(port, vlanID);
       						LOG(4)(Log::MPLS, "VLSR: Removing ingress port#",  GetSwitchPortString(port), "from VLAN #", vlanID);
                                            }
                                            else {
    						       LOG(1)(Log::MPLS, "VLSR: Cannot identify the VLAN (for ingress port) to be operated.");
                                            }

                                            if (vlanID !=  0) {
                                                //increase the bandwidth by the amount taken by the removed LSP (on revserse link for bidirectional LSP only)
                                                //$$$$ To be moved into deleteUpstreamInLabel
                                                RSVP_Global::rsvp->getRoutingService().holdBandwidthbyOSPF(port, (*iter).bandwidth, false); //false == increase

             					     //Undo rate policing and limitation on the port, which is both input and output port
            					     //as the VLAN is duplex.
                                                LOG(4)(Log::MPLS, "VLSR: Undo bandwidth policing and limitation on port#",  GetSwitchPortString(port), "for VLAN #", vlanID);
                                                (*sessionIter)->policeInputBandwidth(false, port, (*iter).vlanTag, (*iter).bandwidth); 
                                                (*sessionIter)->limitOutputBandwidth(false, port, (*iter).vlanTag,  (*iter).bandwidth);//$$$$ To be moved into deleteUpstreamInLabel
                                            }

                                            portList.pop_front();
                                      }
                                      portList.clear();
                                      uint32 outPort = (*iter).outPort;
                                      if ((outPort >> 16) == LOCAL_ID_TYPE_NONE)
                                         portList.push_back(outPort);
                                      else if ((outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                          portList.push_back(outPort & 0xffff);
                                      else
                                      {
                                          DRAGON_UNI_Object* uni = (DRAGON_UNI_Object*)psb.getDRAGON_UNI_Object();
                                          if (uni && uni->getDestTNA().local_id == UNI_AUTO_TAGGED_LCLID)
                                              outPort = RSVP_Global::rsvp->getLocalIdByIfName((char*)uni->getEgressCtrlChannel().name);
                                          SwitchCtrl_Global::getPortsByLocalId(portList, outPort);
                                      }
                                      if (outPort == ((LOCAL_ID_TYPE_TAGGED_GROUP << 16) | 0)) //NULL local-ID
                                      {
                                          portList.clear();
                                      }
                                      else if (portList.size() == 0){
    					      LOG(2)( Log::MPLS, "VLSR: Unrecognized port/localID at egress: ", outPort );
                                         //continue;
                                      }
                                      while (portList.size()) {
                                            uint32 port = portList.front();
                                            vlanID = (*iter).vlanTag;
                                            if (vlanID ==0)
                                                vlanID = (*sessionIter)->getActiveVlanId(port);
                                            if (vlanID != 0) {
                                                (*sessionIter)->removePortFromVLAN(port, vlanID);
       						LOG(4)(Log::MPLS, "VLSR: Removing egress port#",  GetSwitchPortString(port), "from VLAN #", vlanID);
                                            }
                                            else {
    						       LOG(1)(Log::MPLS, "VLSR: Cannot identify the VLAN  (for egress port) to be operated.");
                                            }

                                            if (vlanID !=  0) {
                                                //increase the bandwidth by the amount taken by the removed LSP
                                                RSVP_Global::rsvp->getRoutingService().holdBandwidthbyOSPF(port, (*iter).bandwidth, false); //false == increase

                                                //Undo rate policing and limitation on the port, which is both input and output port
                                                //as the VLAN is duplex.
                                                LOG(4)(Log::MPLS, "VLSR: Undo bandwidth policing and limitation on port#",  GetSwitchPortString(port), "for VLAN #", vlanID);
                                                (*sessionIter)->policeInputBandwidth(false, port, (*iter).vlanTag, (*iter).bandwidth);//$$$$ To be moved into deleteUpstreamInLabel
                                                (*sessionIter)->limitOutputBandwidth(false, port, (*iter).vlanTag,  (*iter).bandwidth);
                                            }
                                            portList.pop_front();
                                      }

                                    if ((*iter).vlanTag != 0)// && !(*sessionIter)->VLANHasTaggedPort((*iter).vlanTag))
                                    {
                                        //restore the VTAG that has been released from removing the VLAN.
                                        if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL) //$$$$ To be moved into deleteUpstreamInLabel
	                                        RSVP_Global::rsvp->getRoutingService().holdVtagbyOSPF((*iter).inPort & 0xffff, (*iter).vlanTag, false); //false == release
                                        if (((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
	                                        RSVP_Global::rsvp->getRoutingService().holdVtagbyOSPF((*iter).outPort & 0xffff, (*iter).vlanTag, false); //false == release
 					}

                                   if ((*sessionIter)->isVLANEmpty(vlanID)) {
                                        if (!(*sessionIter)->removeVLAN(vlanID)) {
                                            LOG(2)(Log::MPLS, "VLSR: Failed to remove the empty VLAN: ",  vlanID);
                                        }
                                   }
 					break;
                        }
			}
			iter = psb.getVLSR_Route().erase(iter);
		}
	}
}

void MPLS::deleteUpstreamInLabel(PSB& psb){
	LOG(2)( Log::MPLS, "MPLS: deleting upstream input label", psb.getUPSTREAM_IN_LABEL_Object().getLabel() );
}

void MPLS::deleteOutLabel( const MPLS_OutLabel* outLabel ) {
	LOG(2)( Log::MPLS, "MPLS: deleting output label", outLabel->getLabel() );
#if defined(MPLS_CAMBRIDGE)
	uint32 filterHandle = outLabel->filter ? outLabel->filter->destAddress.rawAddress() : 0;
#endif
	if ( outLabel->filter ) internDeleteClassifier( outLabel->filter );
#if defined(MPLS_WISCONSIN)
	static struct mpls_out_label_req mol_req;
	initMemoryWithZero( &mol_req, sizeof(mol_req) );
	mol_req.mol_label.ml_type = MPLS_LABEL_GEN;
	mol_req.mol_label.u.ml_gen = outLabel->getLabel();
	mol_req.mol_label.ml_index = outLabel->getLifSysIndex();
	CHECK( send_nhlfe( netlink, &mol_req, RTM_DELNHLFE ) );
#elif defined(MPLS_CAMBRIDGE)
	if (filterHandle) {
		static cid_t cid;
		initMemoryWithZero( &cid, sizeof(cid) );
		cid.port = 0;
		cid.label = filterHandle;
		CHECK( mpls_del_switch_mapping(&cid) );
		static fec_t fec;
		initMemoryWithZero( &fec, sizeof(fec) );
		fec.proto = MPLSPROTO_IPV4;
		fec.u.ipv4.tclassid = filterHandle;
		CHECK( mpls_del_ingress_mapping(&fec) );
	}
#endif
	delete outLabel;
}

void MPLS::deleteUpstreamOutLabel(PSB& psb){
	LOG(2)( Log::MPLS, "MPLS: deleting upstream output label", psb.getUPSTREAM_OUT_LABEL_Object().getLabel() );
}

#if defined(MPLS_CAMBRIDGE)
inline void MPLS::arpLookup( const LogicalInterface& lif, const NetAddress& addr, char r_addr[] ) {
	int fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	static struct arpreq arpRequest;
	initMemoryWithZero( &arpRequest, sizeof(arpRequest) );
	reinterpret_cast<sockaddr_in&>(arpRequest.arp_pa).sin_family = AF_INET;
	reinterpret_cast<sockaddr_in&>(arpRequest.arp_pa).sin_port = 0;
	reinterpret_cast<sockaddr_in&>(arpRequest.arp_pa).sin_addr.s_addr = addr.rawAddress();
	strcpy( arpRequest.arp_dev, lif.getName().chars() );
	CHECK( ioctl( fd, SIOCGARP, &arpRequest ) );
	copyMemory( r_addr, arpRequest.arp_ha.sa_data, sizeof(arpRequest.arp_ha.sa_data) );
	close(fd);
}

uint32 MPLS::createHopInfo( const LogicalInterface& lif, const NetAddress& nhop ) {
	static int portnum = 0;
	static port_mapping_t pm;
	initMemoryWithZero( &pm, sizeof(pm) );
	pm.type = ETH_PORT;
	pm.id = ++portnum;
	strcpy( pm.u.eth.l_ifname, lif.getName().chars() );
	arpLookup( lif, nhop, pm.u.eth.r_addr );
	CHECK( mpls_add_port_mapping( &pm ) );
	return portnum;
}

void MPLS::removeHopInfo( uint32 port ) {
	CHECK( mpls_del_port_mapping( port ) );
}
#endif /* MPLS_CAMBRIDGE */

EXPLICIT_ROUTE_Object* MPLS::getExplicitRoute(NetAddress& dest) {
	EXPLICIT_ROUTE_Object* er = NULL;
	ExplicitRouteList::ConstIterator erIter = erList.find( dest );
	if ( erIter != erList.end() ) {
              assert( !(*erIter).anList.empty() );
		er = new EXPLICIT_ROUTE_Object;
		SimpleList<NetAddress>::ConstIterator addrIter = (*erIter).anList.end();
		for ( ;; ) {
			--addrIter;
			er->pushFront( AbstractNode( false, (*addrIter), (uint8)32 ) );
			if ( addrIter == (*erIter).anList.begin() ) {
		break;
			}
		}
	}

	//destination is local
	if (!er && RSVP_Global::rsvp->getRoutingService().getLoopbackAddress() == dest) {
		er = new EXPLICIT_ROUTE_Object;
              er->pushFront(AbstractNode(false, dest, (uint8)32));
	}
	//query from configured virtual routes...
	if (!er) {
		LogicalInterface* outLif;
		NetAddress nexthop, remote_nexthop, gw;
		uint32 ifID;
		if (RSVP_Global::rsvp->getRoutingService().getRoute(dest, outLif, gw)) {
			if (RSVP_Global::rsvp->getRoutingService().findDataByInterface(*outLif, nexthop, ifID)) {
				er = new EXPLICIT_ROUTE_Object;
				RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(nexthop, remote_nexthop);
				er->pushFront(AbstractNode(false, remote_nexthop, (uint8)32));
			}
		}
	}

	return er;
}

EXPLICIT_ROUTE_Object* MPLS::updateExplicitRoute( const NetAddress& dest, EXPLICIT_ROUTE_Object* er ) {
	if ( er ) er->borrow();
	ExplicitRouteList::ConstIterator erIter = erList.find( dest );
	if ( erIter != erList.end() ) {
                                           assert( !(*erIter).anList.empty() );
		if (!er) 
			er = new EXPLICIT_ROUTE_Object;
		else 
			while (!er->getAbstractNodeList().empty())er->popFront();
		SimpleList<NetAddress>::ConstIterator addrIter = (*erIter).anList.end();
		for ( ;; ) {
			--addrIter;
			er->pushFront( AbstractNode( false, (*addrIter), (uint8)32 ) );
			if ( addrIter == (*erIter).anList.begin() ) {
		break;
			}
		}
	}

	if ((!er || er->getAbstractNodeList().empty()) && RSVP_Global::rsvp->getRoutingService().getLoopbackAddress() == dest) {
		er = new EXPLICIT_ROUTE_Object;
              er->pushFront(AbstractNode(false, dest, (uint8)32));
	}

	if (er) {
		LOG(4)( Log::MPLS, "MPLS: explicit route for", dest, "is", *er );
	}

	return er;
}

void MPLS::addExplicitRoute( const NetAddress& dest, const SimpleList<NetAddress>& alist,  uint32 sid ) {
	ExplicitRoute er(dest, sid);
	er.anList = alist;
	LOG(2)( Log::MPLS, "MPLS: setting explicit route", er );
	erList.insert_sorted( er );
}

void MPLS::deleteExplicitRouteBySession( uint32 sid ) {
	ExplicitRouteList::ConstIterator erIter = erList.begin();
	for ( ; erIter != erList.end(); ++erIter) {
		if ( sid == (*erIter).sessionID)
			erList.erase(erIter);
	}
}

ostream& operator<<( ostream& os, const ExplicitRoute& er ) {
	os << "dest: " << (NetAddress&)er << " via";
	SimpleList<NetAddress>::ConstIterator iter = er.anList.begin();
	for ( ; iter != er.anList.end(); ++iter ) {
		os << " " << *iter;
	}
	return os;
}

