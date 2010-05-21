
/* Copyright (c) 2010, Stefan Eilemann <eile@eyescale.ch> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "server.h"

#include "client.h"
#include "packets.h"

#include <eq/net/command.h>
#include <eq/net/connectionDescription.h>

namespace eq
{
namespace fabric
{

#define CmdFunc net::CommandFunc< Server< S, CFG, NF > >

template< class S, class CFG, class NF >
Server< S, CFG, NF >::Server( NF* nodeFactory )
        : _nodeFactory( nodeFactory )
{
    EQASSERT( nodeFactory );
}

template< class S, class CFG, class NF >
Server< S, CFG, NF >::~Server()
{
    _client = 0;
    EQASSERT( _configs.empty( ));
}

template< class S, class CFG, class NF >
void Server< S, CFG, NF >::setClient( ClientPtr client )
{
    _client = client;
    if( !client )
        return;

    net::CommandQueue* queue = static_cast< S* >( this )->getMainThreadQueue();
    registerCommand( CMD_SERVER_CREATE_CONFIG, 
                     CmdFunc( this, &Server::_cmdCreateConfig ), queue );
    registerCommand( CMD_SERVER_DESTROY_CONFIG, 
                     CmdFunc( this, &Server::_cmdDestroyConfig ), queue );
}

template< class S, class CFG, class NF >
void Server< S, CFG, NF >::_addConfig( CFG* config )
{ 
    EQASSERT( config->getServer() == static_cast< S* >( this ));
    EQASSERT( stde::find( _configs, config ) == _configs.end( ));
    _configs.push_back( config );
}

template< class S, class CFG, class NF >
bool Server< S, CFG, NF >::_removeConfig( CFG* config )
{
    typename ConfigVector::iterator i = stde::find( _configs, config );
    if( i == _configs.end( ))
        return false;

    _configs.erase( i );
    return true;
}

//---------------------------------------------------------------------------
// command handlers
//---------------------------------------------------------------------------
template< class S, class CFG, class NF > net::CommandResult
Server< S, CFG, NF >::_cmdCreateConfig( net::Command& command )
{
    const ServerCreateConfigPacket* packet = 
        command.getPacket<ServerCreateConfigPacket>();
    EQVERB << "Handle create config " << packet << std::endl;
    EQASSERT( packet->proxy.identifier != EQ_ID_INVALID );
    
    CFG* config = _nodeFactory->createConfig( static_cast< S* >( this ));
    net::NodePtr localNode = command.getLocalNode();
    localNode->mapSession( command.getNode(), config, packet->configID );
    config->map( packet->proxy );

    if( packet->requestID != EQ_ID_INVALID )
    {
        ConfigCreateReplyPacket reply( packet );
        command.getNode()->send( reply );
    }

    return net::COMMAND_HANDLED;
}

template< class S, class CFG, class NF > net::CommandResult
Server< S, CFG, NF >::_cmdDestroyConfig( net::Command& command )
{
    const ServerDestroyConfigPacket* packet = 
        command.getPacket<ServerDestroyConfigPacket>();
    EQVERB << "Handle destroy config " << packet << std::endl;
    
    net::NodePtr  localNode  = command.getLocalNode();
    net::Session* session    = localNode->getSession( packet->configID );

    CFG* config = EQSAFECAST( CFG*, session );
    config->unmap();
    EQCHECK( localNode->unmapSession( config ));
    _nodeFactory->releaseConfig( config );

    if( packet->requestID != EQ_ID_INVALID )
    {
        ServerDestroyConfigReplyPacket reply( packet );
        command.getNode()->send( reply );
    }
    return net::COMMAND_HANDLED;
}

template< class S, class CFG, class NF >
std::ostream& operator << ( std::ostream& os, 
                            const Server< S, CFG, NF >& server )
{
    os << base::disableFlush << base::disableHeader << "server " << std::endl;
    os << "{" << std::endl << base::indent;
    
    const net::ConnectionDescriptionVector& cds =
        server.getConnectionDescriptions();
    for( net::ConnectionDescriptionVector::const_iterator i = cds.begin();
         i != cds.end(); ++i )
    {
        net::ConnectionDescriptionPtr desc = *i;
        os << *desc;
    }

    const std::vector< CFG* >& configs = server.getConfigs();
    for( typename std::vector< CFG* >::const_iterator i = configs.begin();
         i != configs.end(); ++i )
    {
        const CFG* config = *i;
        os << *config;
    }

    os << base::exdent << "}"  << base::enableHeader << base::enableFlush
       << std::endl;

    return os;
}

}
}
