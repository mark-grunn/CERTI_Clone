// ----------------------------------------------------------------------------
// CERTI - HLA RunTime Infrastructure
// Copyright (C) 2002, 2003, 2005  ONERA
//
// This file is part of CERTI-libCERTI
//
// CERTI-libCERTI is free software ; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation ; either version 2 of
// the License, or (at your option) any later version.
//
// CERTI-libCERTI is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY ; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program ; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
//
// $Id: InteractionSet.hh,v 3.23 2008/11/08 01:11:23 erk Exp $
// ----------------------------------------------------------------------------

#ifndef _CERTI_INTERACTION_SET_HH
#define _CERTI_INTERACTION_SET_HH

// CERTI headers
#include "certi.hh"
#include "SecurityServer.hh"
#include "Interaction.hh"
#include "TreeNamedAndHandledSet.hh"

// System headers
#include <string>
#include <map>

namespace certi {

class CERTI_EXPORT InteractionSet : public TreeNamedAndHandledSet<Interaction>
{

public:
	InteractionSet(SecurityServer *the_server, bool isRootClassSet=false);
	~InteractionSet();

	/**
	 * Add an interaction class to the set.
	 * @pre theClass should be non NULL
	 * @param[in] theClass the object class to be added
	 * @param[in] parentClass the parent class (may be NULL)
	 */
	void addClass(Interaction *theClass, Interaction *parentClass);

	// --------------------------
	// -- RTI Support Services --
	// --------------------------

	/**
	 * Return the interaction handle associated to name.
	 * @param[in] class_name the name of the interaction for whose handle is requested
	 */
	InteractionClassHandle
	getInteractionClassHandle(const std::string& class_name) const
	throw (NameNotFound);

	std::string
	getInteractionClassName(InteractionClassHandle the_handle) const
	throw (InteractionClassNotDefined);

	ParameterHandle
	getParameterHandle(const char *the_name,
			InteractionClassHandle the_class)
	throw (NameNotFound,
			InteractionClassNotDefined,
			RTIinternalError);

	const std::string&
	getParameterName(ParameterHandle the_handle,
			InteractionClassHandle the_class)
	throw (InteractionParameterNotDefined,
			InteractionClassNotDefined,
			RTIinternalError);

	void killFederate(FederateHandle the_federate)
	throw ();

	// ----------------------------------
	// -- Interaction Class Management --
	// ----------------------------------
	void publish(FederateHandle the_federate_handle,
			InteractionClassHandle the_interaction_handle,
			bool publish)
	throw (FederateNotPublishing,
			InteractionClassNotDefined,
			RTIinternalError,
			SecurityError);

	void subscribe(FederateHandle the_federate_handle,
			InteractionClassHandle the_interaction_handle,
			const RTIRegion *,
			bool subscribe)
	throw (FederateNotSubscribing,
			InteractionClassNotDefined,
			RTIinternalError,
			SecurityError);

	// -------------------------------------
	// -- Interaction Instance Management --
	// -------------------------------------
	void isReady(FederateHandle theFederateHandle,
			InteractionClassHandle theInteraction,
			std::vector <ParameterHandle> &paramArray,
			UShort paramArraySize)
	throw (FederateNotPublishing,
			InteractionClassNotDefined,
			InteractionParameterNotDefined,
			RTIinternalError);

	void broadcastInteraction(FederateHandle theFederateHandle,
			InteractionClassHandle theInteractionHandle,
			std::vector <ParameterHandle> &theParameterList,
			std::vector <ParameterValue_t> &theValueList,
			UShort theListSize,
			FederationTime theTime,
			const RTIRegion *,
			const char *theTag)
	throw (FederateNotPublishing,
			InteractionClassNotDefined,
			InteractionParameterNotDefined,
			RTIinternalError);

	void broadcastInteraction(FederateHandle theFederateHandle,
			InteractionClassHandle theInteractionHandle,
			std::vector <ParameterHandle> &theParameterList,
			std::vector <ParameterValue_t> &theValueList,
			UShort theListSize,
			const RTIRegion *,
			const char *theTag)
	throw (FederateNotPublishing,
			InteractionClassNotDefined,
			InteractionParameterNotDefined,
			RTIinternalError);

private:

	SecurityServer *server ;
};

} // namespace certi

#endif // _CERTI_INTERACTION_SET_HH

// $Id: InteractionSet.hh,v 3.23 2008/11/08 01:11:23 erk Exp $
