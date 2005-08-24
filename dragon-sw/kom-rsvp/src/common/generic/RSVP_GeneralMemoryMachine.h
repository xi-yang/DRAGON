#ifndef _RSVP_GeneralMemoryMachine_h_
#define _RSVP_GeneralMemoryMachine_h_ 1

#if defined(RSVP_MEMORY_MACHINE)

#include "RSVP_System.h"

template <class T>
class GeneralMemoryMachine {
public:
	struct MemNode {
		MemNode* next;
		MemNode* prev;
		char data[sizeof(T)-2*sizeof(MemNode*)];
		MemNode() : next(this), prev(this) {}
	};

protected:
	MemNode endOfList;
#if defined(RSVP_STATS)
	uint32 totalNodes;
	uint32 freeNodes;
#endif

	void insert_end( MemNode* elem ) {
		elem->next = &endOfList;
		elem->prev = endOfList.prev;
		endOfList.prev->next = elem;
		endOfList.prev = elem;
	}

	MemNode* getAndRemoveFront() {
		MemNode* result = endOfList.next;
		endOfList.next = endOfList.next->next;
		endOfList.next->prev = &endOfList;
		return result;
	}

public:

	void addNodes( uint32 nodeCount ) {
#if defined(RSVP_STATS)
		if ( nodeCount != 0 ) cerr << T::name << " memory machine: adding " << nodeCount << " to " << totalNodes << " nodes" << endl;
#endif
		for ( uint32 i = 0; i < nodeCount; i += 1 ) {
			insert_end( new MemNode );
		}
#if defined(RSVP_STATS)
		freeNodes += nodeCount;
		totalNodes += nodeCount;
#endif
	}

	void cleanup( uint32 count = 0 ) {
		if ( count == 0 ) count = freeNodes;
		while ( endOfList.next != &endOfList && count > 0 ) {
			MemNode* node = getAndRemoveFront();
			operator delete( node );
			freeNodes -= 1;
			count -= 1;
		}
	}

	GeneralMemoryMachine() {
                                     assert( sizeof(T) >= 2*sizeof(MemNode*) );
#if defined(RSVP_STATS)
		totalNodes = freeNodes = 0;
#endif
	}

	~GeneralMemoryMachine() {
#if defined(RSVP_STATS)
		cerr << T::name << " memory machine allocated nodes: " << totalNodes << endl;
#endif
		cleanup();
	}

	void* alloc( size_t size ) {
		if ( endOfList.next == &endOfList ) {
                                                      assert( freeNodes == 0 );
#if defined(RSVP_STATS)
			totalNodes += 1;
#endif
			return ::operator new(size);
		}
#if defined(RSVP_STATS)
		freeNodes -= 1;
#endif
		return getAndRemoveFront();
	}

	void dealloc( void* pnt ) {
		insert_end( (MemNode*)pnt );
#if defined(RSVP_STATS)
		freeNodes += 1;
#endif
	}

#if defined(RSVP_STATS)
	void logStats() {
		printSafe( "available %s nodes: %d, used %s nodes: %d\n", T::name, freeNodes, T::name, totalNodes - freeNodes  );
	}
#endif

};
#endif /* RSVP_MEMORY_MACHINE */

#endif /* _RSVP_GeneralMemoryMachine_h_ */
