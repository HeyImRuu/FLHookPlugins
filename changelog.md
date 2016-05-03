Bounty Tracker - TODO:
=
* 
### things that need testing: ###
* check can't submit bounty again same target *(lastIssuer)* (try to add bounty after it is completed on same target)
* please test new cfg writer. bounty is appended at end of add, it deletes the bounty in addto, modifies the string and appends it as new bounty.
* also deletes bounty at the beginning of shipDestroyed, virtual bounty is updated and then appended to cfg.

Noted below:
====

things completed latley:
* check active bounty using bool BTI.active **(done) (tested)**
* added minimum cash and max xtimes values **(done) (tested)**
* replaced player online check with player exists check **(done) (tested)**
* added check if player already has a bounty on them **(done) (tested)**
* added check if player and victim were the same person **(done) ()**
* the issuer of a bounty cannot claim a reward on it **(done) ()**
* added individual protection (lastIssuer)(one person cant keep submitting bounties on one person) **(done) ()**
* made all name strings lowercase **(done)(-)**
* clear bounty when completed **(done) ()**
* added ability to contribute credits to raise a bounty (per remaining contracts) **(done) (tested)**
* added player exists check to bounty view **(done) (tested)**
* realised how to do cmds properly (hooray!)
* added ability to use periods, commas, and scientific notation in cash amount (note: only x million is availiable e.g 2e6 = 2 mill) **(tested)**
* finally added cfg writer. Now bounties can be save to disk! **(done!) note: flserver requires admin priv to write to files. I wonder if this will cause trouble? (testing req.)**
* due to the cfg writer, the bounties are in a seperate cfg file now. any plugin settings are in the default file.(e.g plugin enable.) (note)
* added a minimum required player age to add a bounty. **(done) (testing req.)**
* added timed protection of 1hr between the completion of a bounty **(done) (testing req.)**

useful strings of code
=
    (wchar_t*)Players.GetActiveCharacterName(iClientID) <- gets charname from id