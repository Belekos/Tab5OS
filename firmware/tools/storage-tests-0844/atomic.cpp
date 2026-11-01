#include "AtomicPlatform.h"
#include "Tab5Storage.h"
#include "AtomicProduction.h"
#include <cassert>
#include <cstdio>
static unsigned checks=0;
#define REQUIRE(x) do{++checks;assert(x);}while(0)
void reset(){files.clear();failStage="";failError=0;cleanupFails=false;promoteFails=false;mutations=0;tab5StorageBootResult(true,0);errno=0;auto a=std::make_shared<Node>();a->bytes="original";files["/docs/a.txt"]=a;}
int main(){
 const std::string payload(10000,'N');unsigned lostCauses=0,incorrectRecovery=0;
 reset();REQUIRE(tab5AtomicWrite("/docs/a.txt",payload.data(),payload.size()));
 REQUIRE(files.at("/docs/a.txt")->bytes==payload);REQUIRE(files.at("/docs/.Tab5Backups/a.txt.bak")->bytes=="original");
 REQUIRE(!files.count("/docs/.a.txt.tmp"));
 // The initial file must survive every failure, and errno must retain the
 // original write/sync/close error even if cleanup has a different error.
 for(const char* stage:{"write","flush","close","size"})for(int error:{ENOSPC,ENOMEM,EROFS,EMFILE,ETIMEDOUT,EIO})for(bool cleanup:{false,true}){
  reset();failStage=stage;failError=error;cleanupFails=cleanup;
  REQUIRE(!tab5AtomicWrite("/docs/a.txt",payload.data(),payload.size()));const int returned=errno;
  REQUIRE(files.at("/docs/a.txt")->bytes=="original");
  REQUIRE(!files.count("/docs/.Tab5Backups/a.txt.bak"));
  const bool transport=error==ETIMEDOUT||error==EIO;
  const bool recovered=tab5StorageRecoveryDue(3000);
  if(tab5StorageLastError()!=error||returned!=error)++lostCauses;
  if(recovered&&!transport)++incorrectRecovery;
#ifdef EXPECT_FIXED
  REQUIRE(tab5StorageLastError()==error&&returned==error&&recovered==transport);
#endif
 }
 reset();promoteFails=true;REQUIRE(!tab5AtomicWrite("/docs/a.txt",payload.data(),payload.size()));
 const int promotionError=errno;REQUIRE(files.at("/docs/a.txt")->bytes=="original");
#ifdef EXPECT_FIXED
 REQUIRE(promotionError==ENOSPC);
#endif
 // A failed existence probe is not permission to modify any committed file.
 reset();failStage="probe";failError=EIO;REQUIRE(!tab5AtomicWrite("/docs/a.txt",payload.data(),payload.size()));
 REQUIRE(files.at("/docs/a.txt")->bytes=="original"&&mutations==0);
 std::printf("{\"checks\":%u,\"injectedErrorCases\":48,\"lostErrorCauses\":%u,\"incorrectRecoveryRequests\":%u,\"promotionErrno\":%d}\n",checks,lostCauses,incorrectRecovery,promotionError);
}
