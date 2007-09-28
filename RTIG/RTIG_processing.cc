// ----------------------------------------------------------------------------
// CERTI - HLA RunTime Infrastructure
// Copyright (C) 2002-2005  ONERA
//
// This file is part of CERTI
//
// CERTI is free software ; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation ; either version 2 of the License, or
// (at your option) any later version.
//
// CERTI is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY ; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program ; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// $Id: RTIG_processing.cc,v 3.41 2007/09/28 14:07:54 rousse Exp $
// ----------------------------------------------------------------------------

#include <config.h>
#include "RTIG.hh"

#include <assert.h>

using std::endl ;
using std::cout ;

namespace certi {
namespace rtig {

static PrettyDebug D("RTIG", __FILE__);
static PrettyDebug G("GENDOC",__FILE__);

// ----------------------------------------------------------------------------
//! Creates a new federation.
void
RTIG::processCreateFederation(Socket *link, NetworkMessage *req)
{
    char *federation = req->federationName ;
    char *FEDid = req->FEDid ;

    G.Out(pdGendoc,"BEGIN ** CREATE FEDERATION SERVICE **");
    G.Out(pdGendoc,"enter RTIG::processCreateFederation");

    if (federation == NULL) throw RTIinternalError("Invalid Federation Name.");
    auditServer << "Federation Name : " << federation ;
    Handle h = federationHandles.provide();

#ifdef FEDERATION_USES_MULTICAST
    // multicast base address
    unsigned long base_adr_mc = inet_addr(ADRESSE_MULTICAST);
    SocketMC *com_mc = NULL ;

    // creer la communication multicast
    com_mc = new SocketMC();
    if (com_mc == NULL) {
        D.Out(pdExcept, "Unable to allocate Multicast socket.");
        throw RTIinternalError("Unable to allocate Multicast socket.");
    }

    com_mc->CreerSocketMC(base_adr_mc + h, MC_PORT);

    // inserer la nouvelle federation dans la liste des federations
    federations->createFederation(federation, h, com_mc);

    // inserer descripteur fichier pour le prochain appel a un select
    ClientSockets.push_front(com_mc);

#else
    federations.createFederation(federation, h, FEDid);
#endif

    // Prepare answer for RTIA
    NetworkMessage rep ;
    rep.type = NetworkMessage::CREATE_FEDERATION_EXECUTION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federation = h ;
    strcpy(rep.FEDid,FEDid) ;

    G.Out(pdGendoc,"processCreateFederation===>write");

    rep.write(link); // Send answer to RTIA

    D.Out(pdInit, "Federation \"%s\" created with Handle %d.",
          federation, rep.federation);

    G.Out(pdGendoc,"exit RTIG::processCreateFederation");
    G.Out(pdGendoc,"END ** CREATE FEDERATION SERVICE **");
}

// ----------------------------------------------------------------------------
//! Add a new federate to federation.
void
RTIG::processJoinFederation(Socket *link, NetworkMessage *req)
{
    char *federation = req->federationName ;
    char *federate = req->federateName ;
    char filename[MAX_FEDFILE_NAME_LENGTH + 1] ;
    
    unsigned int peer = req->bestEffortPeer ;
    unsigned long address = req->bestEffortAddress ;

    Handle num_federation ;
    FederateHandle num_federe ;

    int nb_regulateurs ;
    int nb_federes ;
    bool pause ;

    G.Out(pdGendoc,"BEGIN ** JOIN FEDERATION SERVICE **");
    G.Out(pdGendoc,"enter RTIG::processJoinFederation");

    if ((federation == NULL) || (federate == NULL))
        throw RTIinternalError("Invalid Federation/Federate Name.");

    auditServer << "Federate \"" << federate << "\" joins Federation \""
		<< federation << "\"" ;

    federations.exists(federation, num_federation);

    num_federe = federations.addFederate(num_federation,
                                          federate,
                                          (SecureTCPSocket *) link);

#ifdef FEDERATION_USES_MULTICAST
    SocketMC *com_mc = NULL ;

    federations.info(num_federation, nb_federes, nb_regulateurs,
                      pause, com_mc);
    assert(com_mc != NULL);
#else
    federations.info(num_federation, nb_federes, nb_regulateurs, pause, filename);
#endif

    // Store Federate <->Socket reference.
    socketServer.setReferences(link->returnSocket(),
                                num_federation,
                                num_federe,
                                address,
                                peer);

    auditServer << "(" << num_federation << ")with handle " << num_federe
		<< ". Socket " << link->returnSocket();

    // Prepare answer about JoinFederationExecution
    // This answer wille be made AFTER FED file processing
    NetworkMessage rep ;
    rep.type = NetworkMessage::JOIN_FEDERATION_EXECUTION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = num_federe ;
    rep.federation = num_federation ;
    rep.numberOfRegulators = nb_regulateurs ;
    rep.bestEffortPeer = peer ;
    rep.bestEffortAddress = address ;

    // Here begin FED file processing i.e. RTIG gives FED file contents to RTIA
    TypeException e = e_NO_EXCEPTION ;
    // Open FED file and says to RTIA if success
    FILE *fdd ;
    if ( (fdd=fopen(filename,"r")) == NULL )
        {
        // Problem : file has been opened during create federation and now we can't
        // May be file has been deleted       
        cout << "processJoinFederation : FED file " << filename << " has vanished." << endl;
        e = e_RTIinternalError ;
        }

    // RTIG says OK or not to RTIA
    NetworkMessage repFED ;
    repFED.type = NetworkMessage::GET_FED_FILE ;
    repFED.federate = num_federe ;
    repFED.federation = num_federation ;
    repFED.number = 0 ;
    strcpy(repFED.FEDid,filename) ;
    repFED.exception = e ;
    // Send answer
    D.Out(pdTrace,"send NetworkMessage of Type %d after open \"%s\"",
          repFED.type,repFED.FEDid);

    G.Out(pdGendoc,"processJoinFederation====>Begin FED file transfer");
    repFED.write(link);

    if ( e ==  e_NO_EXCEPTION )  
        {
        // Wait for OK from RTIA
        NetworkMessage msg ;
        msg.type = NetworkMessage::GET_FED_FILE ;
        D.Out(pdTrace,"wait NetworkMessage of Type %d",msg.type);
        msg.read(link);
        assert ( msg.number == 0 );
        // RTIA has opened working file then RTIG has to transfer file contents
        // line by line
        char file_line[MAX_BYTES_PER_VALUE+1];  
        int num_line = 0 ;
        while ( fgets(file_line,MAX_BYTES_PER_VALUE,fdd) != NULL )
            {
            num_line++;
            // RTIG sends line to RTIA and number gives line number
            repFED.type = NetworkMessage::GET_FED_FILE ;
            repFED.exception = e_NO_EXCEPTION ;
            repFED.federate = num_federe ;
            repFED.federation = num_federation ;
            repFED.number = num_line ;
            assert ( strlen(filename) <= MAX_FEDFILE_NAME_LENGTH ) ;
            strcpy(repFED.FEDid,filename) ;
            // line transfered
            repFED.handleArraySize = 1 ;
            assert ( strlen(file_line) <= MAX_BYTES_PER_VALUE );
            repFED.setValue(0,file_line,strlen(file_line)+1);  

            // Send answer
            repFED.write(link);

            // Wait for OK from RTIA
            msg.read(link);
            assert ( msg.number == num_line );
            }
    
	// close
	fclose(fdd) ;
        repFED.type = NetworkMessage::GET_FED_FILE ;
        repFED.exception = e_NO_EXCEPTION ;
        repFED.federate = num_federe ;
        repFED.federation = num_federation ;
        repFED.number = 0 ;
        strcpy(repFED.FEDid,filename) ;   

        // Send answer

        G.Out(pdGendoc,"processJoinFederation====>End  FED file transfer");

        repFED.write(link);
        }
    // END of FED file processing

    // Now we have to answer about JoinFederationExecution

#ifdef FEDERATION_USES_MULTICAST
    rep.AdresseMulticast = com_mc->returnAdress();
#endif

    D.Out(pdInit, "Federate \"%s\" has joined Federation %u under handle %u.",
          federate, num_federation, num_federe);

    // Send answer

    G.Out(pdGendoc,"processJoinFederation====>write");

    rep.write(link);

    G.Out(pdGendoc,"exit RTIG::processJoinFederation");
    G.Out(pdGendoc,"END ** JOIN FEDERATION SERVICE **");

}

// ----------------------------------------------------------------------------
//! Removes a federate from federation.
void
RTIG::processResignFederation(Handle federation,
                              FederateHandle federe)
{

    G.Out(pdGendoc,"BEGIN ** RESIGN FEDERATION SERVICE **");
    G.Out(pdGendoc,"enter RTIG::processResignFederation");

    federations.remove(federation, federe);
    D.Out(pdInit, "Federate %u is resigning from federation %u.",
          federe, federation);

    G.Out(pdGendoc,"exit RTIG::processResignFederation");
    G.Out(pdGendoc,"END ** RESIGN FEDERATION SERVICE **");

}

// ----------------------------------------------------------------------------
//! Removes a federation.
void
RTIG::processDestroyFederation(Socket *link, NetworkMessage *req)
{
    Handle num_federation ;
    char *federation = req->federationName ;

    G.Out(pdGendoc,"BEGIN ** DESTROY FEDERATION SERVICE **");
    G.Out(pdGendoc,"enter RTIG::processDestroyFederation");

    if (federation == NULL) throw RTIinternalError("Invalid Federation Name.");

    auditServer << "Name \"" << federation << "\"" ;
    federations.exists(federation, num_federation);
    federations.destroyFederation(num_federation);
    federationHandles.free(num_federation);
    D.Out(pdInit, "Federation \"%s\" has been destroyed.", federation);

    NetworkMessage rep ;
    rep.type = NetworkMessage::DESTROY_FEDERATION_EXECUTION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;

    G.Out(pdGendoc,"processDestroyFederation===>write");

    rep.write(link);

    G.Out(pdGendoc,"exit RTIG::processDestroyFederation");
    G.Out(pdGendoc,"END ** DESTROY FEDERATION SERVICE **");

}

// ----------------------------------------------------------------------------
//! Put federate as time regulating.
void
RTIG::processSetTimeRegulating(NetworkMessage *msg)
{
    if (msg->regulator) {
        auditServer << "ON at time " << msg->date ;

        federations.createRegulator(msg->federation,
                                     msg->federate,
                                     msg->date);
        D.Out(pdTerm, "Federate %u of Federation %u sets TimeRegulation ON.",
              msg->federate, msg->federation);
    }
    else {
        auditServer << "OFF" ;

        federations.removeRegulator(msg->federation,
                                     msg->federate);
        D.Out(pdTerm, "Federate %u of Federation %u sets TimeRegulation OFF.",
              msg->federate, msg->federation);
    }
}

// ----------------------------------------------------------------------------
//! Put federate as time constrained
void
RTIG::processSetTimeConstrained(NetworkMessage *msg)
{
    if (msg->constrained) {
        auditServer << "ON at time " << msg->date ;

        federations.addConstrained(msg->federation,
                                    msg->federate);
        D.Out(pdTerm, "Federate %u of Federation %u is now constrained.",
              msg->federate, msg->federation);
    }
    else {
        auditServer << "OFF" ;

        federations.removeConstrained(msg->federation,
                                       msg->federate);
        D.Out(pdTerm, "Federate %u of Federation %u is no more constrained.",
              msg->federate, msg->federation);
    }
}

// ----------------------------------------------------------------------------
//! processMessageNull.
void
RTIG::processMessageNull(NetworkMessage *msg)
{
    auditServer << "Date " << msg->date ;

    // Catch all exceptions because RTIA does not expect an answer anyway.
    try {
        federations.updateRegulator(msg->federation,
                                     msg->federate,
                                     msg->date);
    } catch (Exception &e) {}
}

// ----------------------------------------------------------------------------
//! processRegisterSynchronization.
void
RTIG::processRegisterSynchronization(Socket *link, NetworkMessage *req)
{

    G.Out(pdGendoc,"BEGIN ** REGISTER FEDERATION SYNCHRONIZATION POINT Service **");
    G.Out(pdGendoc,"enter RTIG::processRegisterSynchronization");

    auditServer << "Label \"" << req->label << "\" registered. Tag is \""
		<< req->tag << "\"" ;

    // boolean true means a federates set exists
    if ( req->boolean )
        federations.manageSynchronization(req->federation,
                                          req->federate,
                                          true,
                                          req->label,
                                          req->tag,
                                          req->handleArraySize,
                                          req->handleArray);
    else
        federations.manageSynchronization(req->federation,
                                          req->federate,
                                          true,
                                          req->label,
                                          req->tag);
    D.Out(pdTerm, "Federation %u is now synchronizing.", req->federation);

    // send synchronizationPointRegistrationSucceeded() to federate.
    NetworkMessage rep ;
    rep.type = NetworkMessage::SYNCHRONIZATION_POINT_REGISTRATION_SUCCEEDED ;
    rep.federate = req->federate ;
    rep.federation = req->federation ;
    rep.setLabel(req->label);

    G.Out(pdGendoc,"      processRegisterSynchronization====> write SPRS to RTIA");

    rep.write(link);

    // boolean true means a federates set exists
    if ( req->boolean )
        federations.broadcastSynchronization(req->federation,
                                          req->federate,
                                          req->label,
                                          req->tag,
                                          req->handleArraySize,
                                          req->handleArray);
    else
        federations.broadcastSynchronization(req->federation,
                                          req->federate,
                                          req->label,
                                          req->tag);

    G.Out(pdGendoc,"exit  RTIG::processRegisterSynchronization");
    G.Out(pdGendoc,"END   ** REGISTER FEDERATION SYNCHRONIZATION POINT Service **");

}

// ----------------------------------------------------------------------------
//! processSynchronizationAchieved.
void
RTIG::processSynchronizationAchieved(Socket *, NetworkMessage *req)
{
    auditServer << "Label \"" << req->label << "\" ended." ;

    federations.manageSynchronization(req->federation,
                                       req->federate,
                                       false,
                                       req->label,
                                       "");
    D.Out(pdTerm, "Federate %u has synchronized.", req->federate);
}

// ----------------------------------------------------------------------------
void
RTIG::processRequestFederationSave(Socket *, NetworkMessage *req)
{
    G.Out(pdGendoc,"BEGIN ** REQUEST FEDERATION SAVE SERVICE **");
    G.Out(pdGendoc,"enter RTIG::processRequestFederationSave");

    auditServer << "Federation save request." ;

    if ( req->boolean )
        // With time
        federations.requestFederationSave(req->federation, req->federate,
                                          req->label, req->date);
    else
        // Without time
        federations.requestFederationSave(req->federation, req->federate,
                                          req->label);

    G.Out(pdGendoc,"exit  RTIG::processRequestFederationSave");
    G.Out(pdGendoc,"END   ** REQUEST FEDERATION SAVE SERVICE **");
}
// ----------------------------------------------------------------------------
void
RTIG::processFederateSaveBegun(Socket *, NetworkMessage *req)
{
    auditServer << "Federate " << req->federate << " save begun." ;

    federations.federateSaveBegun(req->federation, req->federate);
}

// ----------------------------------------------------------------------------
void
RTIG::processFederateSaveStatus(Socket *, NetworkMessage *req)
{
    auditServer << "Federate " << req->federate << " save ended." ;

    bool status = req->type == NetworkMessage::FEDERATE_SAVE_COMPLETE ;
    federations.federateSaveStatus(req->federation, req->federate, status);
}

// ----------------------------------------------------------------------------
void
RTIG::processRequestFederationRestore(Socket *, NetworkMessage *req)
{
    auditServer << "Federate " << req->federate << " request restore." ;

    federations.requestFederationRestore(req->federation, req->federate,
                                          req->label);
}

// ----------------------------------------------------------------------------
void
RTIG::processFederateRestoreStatus(Socket *, NetworkMessage *req)
{
    auditServer << "Federate " << req->federate << " restore ended." ;

    bool status = req->type == NetworkMessage::FEDERATE_RESTORE_COMPLETE ;
    federations.federateRestoreStatus(req->federation, req->federate, status);
}

// ----------------------------------------------------------------------------
// processPublishObjectClass
void
RTIG::processPublishObjectClass(Socket *link, NetworkMessage *req)
{
    bool pub = (req->type == NetworkMessage::PUBLISH_OBJECT_CLASS);

    auditServer << "Class = " << req->objectClass << ", # of att. = " 
		<< req->handleArraySize ;

    federations.publishObject(req->federation,
                               req->federate,
                               req->objectClass,
                               req->handleArray,
                               req->handleArraySize,
                               pub);

    D.Out(pdRegister, "Federate %u of Federation %u published object class %d.",
          req->federate, req->federation, req->objectClass);

    NetworkMessage rep ;
    rep.type = req->type ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.objectClass = req->objectClass ;
    rep.handleArraySize = 0 ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processSubscribeObjectClass
void
RTIG::processSubscribeObjectClass(Socket *link, NetworkMessage *req)
{
    bool sub = (req->type == NetworkMessage::SUBSCRIBE_OBJECT_CLASS);

    auditServer << "Class = " << req->objectClass
		<< ", # of att. = " << req->handleArraySize ;

    federations.subscribeObject(req->federation,
				req->federate,
				req->objectClass,
				sub ? req->handleArray : 0,
				sub ? req->handleArraySize : 0);

    D.Out(pdRegister,
          "Federate %u of Federation %u subscribed to object class %d.",
          req->federate, req->federation, req->objectClass);

    NetworkMessage rep ;
    rep.type = req->type ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.objectClass = req->objectClass ;
    rep.handleArraySize = 0 ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processPublishInteractionClass
void
RTIG::processPublishInteractionClass(Socket *link, NetworkMessage *req)
{
    assert(link != NULL && req != NULL);

    bool pub = (req->type == NetworkMessage::PUBLISH_INTERACTION_CLASS);

    auditServer << "Class = " << req->interactionClass ;
    federations.publishInteraction(req->federation,
                                    req->federate,
                                    req->interactionClass,
                                    pub);
    D.Out(pdRequest, "Federate %u of Federation %u publishes Interaction %d.",
          req->federate,
          req->federation,
          req->interactionClass);

    NetworkMessage rep ;
    rep.type = req->type ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.interactionClass = req->interactionClass ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processSubscribeInteractionClass
void
RTIG::processSubscribeInteractionClass(Socket *link, NetworkMessage *req)
{
    bool sub = (req->type == NetworkMessage::SUBSCRIBE_INTERACTION_CLASS);

    auditServer << "Class = %u" << req->interactionClass ;
    federations.subscribeInteraction(req->federation,
                                      req->federate,
                                      req->interactionClass,
                                      sub);
    D.Out(pdRequest,
          "Federate %u of Federation %u subscribed to Interaction %d.",
          req->federate,
          req->federation,
          req->interactionClass);

    NetworkMessage rep ;
    rep.type = req->type ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.interactionClass = req->interactionClass ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processRegisterObject
void
RTIG::processRegisterObject(Socket *link, NetworkMessage *req)
{
    NetworkMessage rep ;

    auditServer << "Class = %u" << req->objectClass ;
    rep.object = federations.registerObject(req->federation,
                                             req->federate,
                                             req->objectClass,
                                             req->label);
    auditServer << ", Handle = " << rep.object ;

    D.Out(pdRegister,
          "Object \"%s\" of Federate %u has been registered under ID %u.",
          req->label, req->federate, rep.object);

    rep.type = req->type ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.setLabel(req->label);
    // rep.object is set by the call of registerObject

    rep.write(link); // Send answer to RTIA
}

// ----------------------------------------------------------------------------
// processUpdateAttributeValues
void
RTIG::processUpdateAttributeValues(Socket *link, NetworkMessage *req)
{
    ValueLengthPair *ValueArray = NULL ;

    auditServer << "ObjID = " << req->object
		<< ", Date = " << req->date ;

    // Get Value Array
    ValueArray = req->getAttribValueArray();

    // Forward the call
    if ( req->getBoolean() )
        {
        // UAV with time
        federations.updateAttribute(req->federation,
                                 req->federate,
                                 req->object,
                                 req->handleArray,
                                 ValueArray,
                                 req->handleArraySize,
                                 req->date,
                                 req->label);
        }
    else
        {
        // UAV without time
        federations.updateAttribute(req->federation,
                                 req->federate,
                                 req->object,
                                 req->handleArray,
                                 ValueArray,
                                 req->handleArraySize,
                                 req->label);
        }
    free(ValueArray);

    // Building answer (Network Message re)
    NetworkMessage rep ;
    rep.type = NetworkMessage::UPDATE_ATTRIBUTE_VALUES ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;
    rep.date = req->date ;
    rep.handleArraySize = 0 ;
    // Don't forget label and tag
    strcpy(rep.label,req->label) ;
    strcpy(rep.tag,req->tag) ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processSendInteraction
void
RTIG::processSendInteraction(Socket *link, NetworkMessage *req)
{
    ValueLengthPair *values = NULL ;

    G.Out(pdGendoc,"BEGIN ** SEND INTERACTION SERVICE **");
    G.Out(pdGendoc,"enter RTIG::processSendInteraction");

    // Building Value Array
    auditServer << "IntID = " << req->interactionClass
		<< ", date = " << req->date ;
    values = req->getParamValueArray();

    if ( req->getBoolean() )
        {
        federations.updateParameter(req->federation,
				req->federate,
				req->interactionClass,
				req->handleArray,
				values,
				req->handleArraySize,
				req->date,
				req->region,
				req->label);
        }
    else
        {
        federations.updateParameter(req->federation,
				req->federate,
				req->interactionClass,
				req->handleArray,
				values,
				req->handleArraySize,
				req->region,
				req->label);
        }
    free(values);

    D.Out(pdDebug, "Interaction %d parameters update completed",
          req->interactionClass);

    NetworkMessage rep ;
    rep.type = NetworkMessage::SEND_INTERACTION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.interactionClass = req->interactionClass ;
    rep.handleArraySize = 0 ;
    // Don't forget label and tag
    strcpy(rep.label,req->label) ;
    strcpy(rep.tag,req->tag) ;
    G.Out(pdGendoc,"processSendInteraction===>write");
    rep.write(link); // send answer to RTIA

    G.Out(pdGendoc,"exit RTIG::processSendInteraction");
    G.Out(pdGendoc,"END ** SEND INTERACTION SERVICE **");

}

// ----------------------------------------------------------------------------
// processdeleteObject
void
RTIG::processDeleteObject(Socket *link, NetworkMessage *req)
{
    auditServer << "ObjID = %u" << req->object ;

    federations.destroyObject(req->federation,
                               req->federate,
                               req->object,
                               req->label);

    D.Out(pdRegister, "Object # %u of Federation %u has been deleted.",
          req->object, req->federation);

    NetworkMessage rep ;
    rep.type = NetworkMessage::DELETE_OBJECT ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processqueryAttributeOwnership
void
RTIG::processQueryAttributeOwnership(Socket *link, NetworkMessage *req)
{
    D.Out(pdDebug, "Owner of Attribute %u of Object %u .",
          req->handleArray[0], req->object);

    auditServer << "AttributeHandle = " << req->handleArray[0] ;

    federations.searchOwner(req->federation,
                             req->federate,
                             req->object,
                             req->handleArray[0]);

    D.Out(pdDebug, "Owner of Attribute %u of Object %u .",
          req->handleArray[0], req->object);

    NetworkMessage rep ;
    rep.type = NetworkMessage::QUERY_ATTRIBUTE_OWNERSHIP ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processAttributeOwnedByFederate
void
RTIG::processAttributeOwnedByFederate(Socket *link, NetworkMessage *req)
{
    NetworkMessage rep ;

    D.Out(pdDebug, "Owner of Attribute %u of Object %u .",
          req->handleArray[0], req->object);

    auditServer << "AttributeHandle = " << req->handleArray[0] ;

    if (federations.isOwner(req->federation,
                             req->federate,
                             req->object,
                             req->handleArray[0]))
        strcpy(rep.label, "RTI_TRUE");
    else
        strcpy(rep.label, "RTI_FALSE");

    D.Out(pdDebug, "Owner of Attribute %u of Object %u .",
          req->handleArray[0], req->object);

    rep.type = NetworkMessage::IS_ATTRIBUTE_OWNED_BY_FEDERATE ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processNegotiatedOwnershipDivestiture
void
RTIG::processNegotiatedOwnershipDivestiture(Socket *link, NetworkMessage *req)
{
    auditServer << "Object = " <<  req->object
		<< ", # of att. = %u" << req->handleArraySize ;
    federations.negotiateDivestiture(req->federation,
                                      req->federate,
                                      req->object,
                                      req->handleArray,
                                      req->handleArraySize,
                                      req->label);

    D.Out(pdDebug, "Federate %u of Federation %u negotiate "
          "divestiture of object %u.",
          req->federate, req->federation, req->object);

    NetworkMessage rep ;
    rep.type = NetworkMessage::NEGOTIATED_ATTRIBUTE_OWNERSHIP_DIVESTITURE ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;
    rep.handleArraySize = 0 ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processAcquisitionIfAvailable
void
RTIG::processAcquisitionIfAvailable(Socket *link, NetworkMessage *req)
{
    auditServer << "Object = " << req->object
		<< ", # of att. = " << req->handleArraySize ;

    federations.acquireIfAvailable(req->federation,
                                    req->federate,
                                    req->object,
                                    req->handleArray,
                                    req->handleArraySize);

    D.Out(pdDebug,
          "Federate %u of Federation %u acquisitionIfAvailable "
          "of object %u.",
          req->federate, req->federation, req->object);

    NetworkMessage rep ;
    rep.type = NetworkMessage::ATTRIBUTE_OWNERSHIP_ACQUISITION_IF_AVAILABLE ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;
    rep.handleArraySize = 0 ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processUnconditionalDivestiture
void
RTIG::processUnconditionalDivestiture(Socket *link, NetworkMessage *req)
{
    auditServer << "Object = " << req->object
		<< ", # of att. = %u" << req->handleArraySize ;

    federations.divest(req->federation,
                        req->federate,
                        req->object,
                        req->handleArray,
                        req->handleArraySize);

    D.Out(pdDebug,
          "Federate %u of Federation %u UnconditionalDivestiture "
          "of object %u.",
          req->federate, req->federation, req->object);

    NetworkMessage rep ;
    rep.type = NetworkMessage::UNCONDITIONAL_ATTRIBUTE_OWNERSHIP_DIVESTITURE ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;
    rep.handleArraySize = 0 ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processOwnershipAcquisition
void
RTIG::processOwnershipAcquisition(Socket *link, NetworkMessage *req)
{
    auditServer << "Object = " << req->object
		<< ", # of att. = %u" << req->handleArraySize ;

    federations.acquire(req->federation,
                         req->federate,
                         req->object,
                         req->handleArray,
                         req->handleArraySize,
                         req->label);

    D.Out(pdDebug,
          "Federate %u of Federation %u ownership acquisition of object %u.",
          req->federate, req->federation, req->object);

    NetworkMessage rep ;
    rep.type = NetworkMessage::ATTRIBUTE_OWNERSHIP_ACQUISITION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;
    rep.handleArraySize = 0 ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processCancelNegotiatedDivestiture
void
RTIG::processCancelNegotiatedDivestiture(Socket *link, NetworkMessage *req)
{
    auditServer << "Object = " << req->object
		<< ", # of att. = " << req->handleArraySize ;

    federations.cancelDivestiture(req->federation,
                                   req->federate,
                                   req->object,
                                   req->handleArray,
                                   req->handleArraySize);

    D.Out(pdDebug, "Federate %u of Federation %u cancel negotiate "
          "divestiture of object %u.",
          req->federate, req->federation, req->object);

    NetworkMessage rep ;
    rep.type = NetworkMessage::CANCEL_NEGOTIATED_ATTRIBUTE_OWNERSHIP_DIVESTITURE ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;
    rep.handleArraySize = 0 ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processReleaseResponse
void
RTIG::processReleaseResponse(Socket *link, NetworkMessage *req)
{
    auditServer << "Object = " << req->object
		<< ", # of att. = " << req->handleArraySize ;

    AttributeHandleSet *attributes =
        federations.respondRelease(req->federation,
                                    req->federate,
                                    req->object,
                                    req->handleArray,
                                    req->handleArraySize);

    D.Out(pdDebug, "Federate %u of Federation %u release response "
          "of object %u.",
          req->federate, req->federation, req->object);

    NetworkMessage rep ;
    rep.handleArraySize = attributes->size();

    for (unsigned int i = 0 ; i < attributes->size(); i++) {
        rep.handleArray[i] = attributes->getHandle(i);
    }

    rep.type = NetworkMessage::ATTRIBUTE_OWNERSHIP_RELEASE_RESPONSE ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;

    rep.write(link); // Send answer to RTIA
}

// ----------------------------------------------------------------------------
// processCancelAcquisition
void
RTIG::processCancelAcquisition(Socket *link, NetworkMessage *req)
{
    auditServer << "Object = " << req->object
		<< ", # of att. = " << req->handleArraySize ;

    federations.cancelAcquisition(req->federation,
                                   req->federate,
                                   req->object,
                                   req->handleArray,
                                   req->handleArraySize);

    D.Out(pdDebug,
          "Federate %u of Federation %u release response of object %u.",
          req->federate, req->federation, req->object);

    NetworkMessage rep ;
    rep.type = NetworkMessage::CANCEL_ATTRIBUTE_OWNERSHIP_ACQUISITION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.object = req->object ;
    rep.handleArraySize = 0 ;

    rep.write(link); // send answer to RTIA
}

// ----------------------------------------------------------------------------
// processCreateRegion
void
RTIG::processCreateRegion(Socket *link, NetworkMessage *req)
{
    // TODO: audit...

    NetworkMessage rep ;

    rep.region = federations.createRegion(req->federation,
                                           req->federate,
                                           req->space,
                                           req->nbExtents);

    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " creates region " << rep.region
               << endl ;

    rep.type = NetworkMessage::DDM_CREATE_REGION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processModifyRegion
void
RTIG::processModifyRegion(Socket *link, NetworkMessage *req)
{
    federations.modifyRegion(req->federation, req->federate,
			      req->region, req->getExtents());
    
    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " modifies region " << req->region
               << endl ;

    NetworkMessage rep ;

    rep.type = NetworkMessage::DDM_MODIFY_REGION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processDeleteRegion
void
RTIG::processDeleteRegion(Socket *link, NetworkMessage *req)
{
    // TODO: audit...

    federations.deleteRegion(req->federation, req->federate, req->region);

    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " deletes region " << req->region
               << endl ;

    NetworkMessage rep ;
    rep.type = NetworkMessage::DDM_DELETE_REGION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.region = req->region ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processAssociateRegion
void
RTIG::processAssociateRegion(Socket *link, NetworkMessage *req)
{
    // TODO: audit...

    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " associates region " << req->region
               << " to some attributes of object " << req->object << endl ;

    federations.associateRegion(req->federation, req->federate, req->object,
				 req->region, req->handleArraySize,
				 req->handleArray);

    NetworkMessage rep ;
    rep.type = NetworkMessage::DDM_ASSOCIATE_REGION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processUnassociateRegion
void
RTIG::processUnassociateRegion(Socket *link, NetworkMessage *req)
{
    // TODO: audit...

    federations.unassociateRegion(req->federation, req->federate,
				   req->object, req->region);

    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " associates region " << req->region
               << " from object " << req->object << endl ;

    NetworkMessage rep ;
    rep.type = NetworkMessage::DDM_UNASSOCIATE_REGION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processSubscribeAttributes
void
RTIG::processSubscribeAttributesWR(Socket *link, NetworkMessage *req)
{
    // TODO: audit...
    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " subscribes with region " << req->region
               << " to some attributes of class " << req->objectClass << endl ;

    federations.subscribeAttributesWR(req->federation, req->federate,
				       req->objectClass, req->region,
				       req->handleArraySize, req->handleArray);

    NetworkMessage rep ;
    rep.type = NetworkMessage::DDM_SUBSCRIBE_ATTRIBUTES ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.objectClass = req->objectClass ;
    rep.handleArraySize = 0 ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processUnsubscribeAttributes
void
RTIG::processUnsubscribeAttributesWR(Socket *link, NetworkMessage *req)
{
    // TODO: audit...
    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " unsubscribes with region " << req->region
               << " from object class " << req->objectClass << endl ;

    federations.unsubscribeAttributesWR(req->federation, req->federate,
					 req->objectClass, req->region);

    NetworkMessage rep ;
    rep.type = NetworkMessage::DDM_UNSUBSCRIBE_ATTRIBUTES ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processSubscribeInteractions
void
RTIG::processSubscribeInteractionWR(Socket *link, NetworkMessage *req)
{
    // TODO: audit...

    federations.subscribeInteractionWR(req->federation, req->federate,
					req->interactionClass, req->region);

    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " subscribes with region " << req->region
               << " to interaction class " << req->interactionClass << endl ;

    NetworkMessage rep ;
    rep.type = NetworkMessage::DDM_SUBSCRIBE_INTERACTION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processUnsubscribeInteractions
void
RTIG::processUnsubscribeInteractionWR(Socket *link, NetworkMessage *req)
{
    // TODO: audit...

    federations.unsubscribeInteractionWR(req->federation, req->federate,
				   req->interactionClass, req->region);

    D[pdDebug] << "Federate " << req->federate << " of Federation "
               << req->federation << " unsubscribes with region " << req->region
               << " from interaction class " << req->interactionClass << endl ;

    NetworkMessage rep ;
    rep.type = NetworkMessage::DDM_UNSUBSCRIBE_INTERACTION ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;
    rep.write(link);
}

// ----------------------------------------------------------------------------
// processRegisterObjectWithRegion
void
RTIG::processRegisterObjectWithRegion(Socket *link, NetworkMessage *req)
{
    NetworkMessage rep ;
    // FIXME bug #9869
    // When we were passed a set of region
    // we should register object for each region  
    // the trouble comes from the fact that
    // there is both
    //     - req->region  (coming from NetworkMessage::region)
    //     - req->regions (coming from BasicMessage::regions)
    // would be nice to sort those thing out.
    rep.object = federations.registerObjectWithRegion(req->federation,
						      req->federate,
						      req->objectClass,
						      req->label,
						      req->region,
						      req->handleArraySize,
						      req->handleArray);
	
    D.Out(pdRegister,
          "Object \"%s\" of Federate %u has been registered under ID %u.",
          req->label, req->federate, rep.object);

    rep.type = NetworkMessage::DDM_REGISTER_OBJECT ;
    rep.type = req->type ;
    rep.exception = e_NO_EXCEPTION ;
    rep.federate = req->federate ;

    rep.write(link); // Send answer to RTIA
}

}} // namespace certi/rtig

// $Id: RTIG_processing.cc,v 3.41 2007/09/28 14:07:54 rousse Exp $
