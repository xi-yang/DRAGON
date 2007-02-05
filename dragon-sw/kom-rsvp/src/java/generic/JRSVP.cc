#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"
#include "JRSVP.h"
static GenericUpcallParameter *gup;

class RSVP_API_Java : public RSVP_API {
	JNIEnv* jEnv;
	jobject japi;

	//static GenericUpcallParameter *gup;

	virtual void preUpcall() {
		jclass classJRSVP = jEnv->GetObjectClass( japi );
		jmethodID preUpcallMethod = jEnv->GetMethodID( classJRSVP, "preUpcall", "()V" );
		jEnv->CallNonvirtualVoidMethod( japi, classJRSVP, preUpcallMethod );
	}

	virtual void postUpcall() {
		jclass classJRSVP = jEnv->GetObjectClass( japi );
		jmethodID postUpcallMethod = jEnv->GetMethodID( classJRSVP, "postUpcall", "()V" );
		jEnv->CallNonvirtualVoidMethod( japi, classJRSVP, postUpcallMethod );
	}

public:
	RSVP_API_Java( uint16 port, const String& rsvpdHost, JNIEnv* jEnv, jobject japi )
		: RSVP_API( port, rsvpdHost ), jEnv(jEnv), japi(japi) {}

	void setJAVA_Env( JNIEnv* jEnv, jobject japi ) {
		this->jEnv = jEnv;
		this->japi = japi;
	}

	static void upcall( const GenericUpcallParameter& upcallPara, RSVP_API_Java* This ) {
		jclass classJRSVP;
		jmethodID upcall;
		gup = new GenericUpcallParameter(upcallPara);
		switch( upcallPara.generalInfo->infoType ) {
		case UpcallParameter::PATH_EVENT:
			classJRSVP = This->jEnv->GetObjectClass( This->japi );
			upcall = This->jEnv->GetMethodID( classJRSVP, "upcallPathEvent", "()V" );
			This->jEnv->CallNonvirtualVoidMethod( This->japi, classJRSVP, upcall );
			break;
		case UpcallParameter::RESV_EVENT:
			classJRSVP = This->jEnv->GetObjectClass( This->japi );
			upcall = This->jEnv->GetMethodID( classJRSVP, "upcallResvEvent", "()V" );
			This->jEnv->CallNonvirtualVoidMethod( This->japi, classJRSVP, upcall );
			break;
		default:
			break;
		}
	}
};

extern "C"
JNIEXPORT jint JNICALL Java_de_tud_kom_rsvp_JRSVP_createAPI
	(JNIEnv * jEnv, jobject japi, jint port, jstring rsvpdHost ) {

	const char* addr = jEnv->GetStringUTFChars( rsvpdHost, 0 );
	long retval = (long)new RSVP_API_Java( port, addr, jEnv, japi );
	jEnv->ReleaseStringUTFChars( rsvpdHost, addr );
	return retval;
}

extern "C"
JNIEXPORT void JNICALL Java_de_tud_kom_rsvp_JRSVP_destroyAPI
	(JNIEnv *, jobject, jint api) {
	delete reinterpret_cast<RSVP_API_Java*>(api);
}

extern "C"
JNIEXPORT jint JNICALL Java_de_tud_kom_rsvp_JRSVP_createSessionInternal
	(JNIEnv * jEnv, jobject, jint api, jstring addr_str, jint protocol, jint port) {

	const char* addr = jEnv->GetStringUTFChars( addr_str, 0 );
	RSVP_API::SessionId* session = new RSVP_API::SessionId;
	*session = ((RSVP_API*)api)->createSession( NetAddress(addr), (uint8)protocol, (uint16)port, (UpcallProcedure)RSVP_API_Java::upcall, (RSVP_API*)api );
	jEnv->ReleaseStringUTFChars( addr_str, addr );
	return (long)session;
}

extern "C"
JNIEXPORT void JNICALL Java_de_tud_kom_rsvp_JRSVP_releaseSessionInternal
	(JNIEnv *, jobject, jint api, jint session) {

	((RSVP_API*)api)->releaseSession( *(RSVP_API::SessionId*)session );
	delete (RSVP_API::SessionId*)session;
}

extern "C"
JNIEXPORT void JNICALL Java_de_tud_kom_rsvp_JRSVP_runAPI
	(JNIEnv * jEnv, jobject japi, jint api ) {

	reinterpret_cast<RSVP_API_Java*>(api)->setJAVA_Env( jEnv, japi );
	reinterpret_cast<RSVP_API*>(api)->run();
}

extern "C"
JNIEXPORT void Java_de_tud_kom_rsvp_JRSVP_createSenderInternal
	(JNIEnv * jEnv, jobject, jint api, jint session, jstring sender_addr, jint port, jobject tspec, jint TTL) {

	if ( tspec == NULL ) return;
	jclass classTSpec = jEnv->GetObjectClass( tspec );
	jfieldID fr = jEnv->GetFieldID( classTSpec, "r", "F" );
	ieee32float r = (ieee32float)jEnv->GetFloatField( tspec, fr );
	jfieldID fp = jEnv->GetFieldID( classTSpec, "p", "F" );
	ieee32float p = (ieee32float)jEnv->GetFloatField( tspec, fp );
	jfieldID fb = jEnv->GetFieldID( classTSpec, "b", "F" );
	ieee32float b = (ieee32float)jEnv->GetFloatField( tspec, fb );
	jfieldID fm = jEnv->GetFieldID( classTSpec, "m", "I" );
	sint32 m = (sint32)jEnv->GetIntField( tspec, fm );
	jfieldID fM = jEnv->GetFieldID( classTSpec, "M", "I" );
	sint32 M = (sint32)jEnv->GetIntField( tspec, fM );

	const char* addr = jEnv->GetStringUTFChars( sender_addr, 0 );
	((RSVP_API*)api)->createSender( *(RSVP_API::SessionId*)session, NetAddress(addr), port, TSpec(r,p,b,m,M), LABEL_REQUEST_Object(), NULL, NULL, NULL, NULL, NULL, TTL );
	jEnv->ReleaseStringUTFChars( sender_addr, addr );
}
     
extern "C"
JNIEXPORT void JNICALL Java_de_tud_kom_rsvp_JRSVP_createReservationInternal
	(JNIEnv * jEnv, jobject, jint api, jint session, jobject fspec, 
	jboolean confirmReservation, jint filterStyle) { 

	if ( fspec == NULL ) return;
	jclass classFlowspec = jEnv->GetObjectClass( fspec );
	jfieldID ftspec = jEnv->GetFieldID( classFlowspec, "tspec", "Lde/tud/kom/rsvp/TSpec;" );
	jobject tspec = jEnv->GetObjectField( fspec, ftspec );

	if ( tspec == NULL ) return;
	jclass classTSpec = jEnv->GetObjectClass( tspec );
	jfieldID fr = jEnv->GetFieldID( classTSpec, "r", "F" );
	ieee32float r = (ieee32float)jEnv->GetFloatField( tspec, fr );
	jfieldID fp = jEnv->GetFieldID( classTSpec, "p", "F" );
	ieee32float p = (ieee32float)jEnv->GetFloatField( tspec, fp );
	jfieldID fb = jEnv->GetFieldID( classTSpec, "b", "F" );
	ieee32float b = (ieee32float)jEnv->GetFloatField( tspec, fb );
	jfieldID fm = jEnv->GetFieldID( classTSpec, "m", "I" );
	sint32 m = (sint32)jEnv->GetIntField( tspec, fm );
	jfieldID fM = jEnv->GetFieldID( classTSpec, "M", "I" );
	sint32 M = (sint32)jEnv->GetIntField( tspec, fM );
	jfieldID frspec = jEnv->GetFieldID( classFlowspec, "rspec", "Lde/tud/kom/rsvp/RSpec;" );
	jobject rspec = jEnv->GetObjectField( fspec, frspec );

	ieee32float R = 0;
	sint32 S = 0;
	if ( rspec != NULL ) {
		jclass classRSpec = jEnv->GetObjectClass( rspec );
		jfieldID fR = jEnv->GetFieldID( classRSpec, "R", "F" );
		R = (ieee32float)jEnv->GetFloatField( rspec, fR );
		jfieldID fS = jEnv->GetFieldID( classRSpec, "S", "I" );
		S = (sint32)jEnv->GetIntField( rspec, fS );
	}

	FLOWSPEC_Object* flowspec = NULL;
	if ( R == 0 ) {
		flowspec = new FLOWSPEC_Object( TSpec(r,p,b,m,M) );
	} else {
		flowspec = new FLOWSPEC_Object( TSpec(r,p,b,m,M), RSpec(R,S) );
	}
	FlowDescriptorList flist;
	flist.push_back( flowspec );
	flist.back().filterSpecList.push_back( gup->pathEvent->senderTemplate );
	((RSVP_API*)api)->createReservation( *(RSVP_API::SessionId*)session, false, FF, flist ); 
}
