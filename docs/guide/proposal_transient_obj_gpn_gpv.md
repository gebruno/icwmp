# Proposal for GPN and GPV of transient object

## Proposal 1

Expose the transient nature data elements on USP only, thus it will restrict
these DM object's visibility on ACS.

### Cons

Customer need to switch to USP to access these DM parameters.

### Effort

Low


## Proposal 2

Maintain a cache of whole DM (Device.) each time a session is started and for
GPV and GPN create the response based on the data available in the cache.
For SPV and Add/Delete object request perform the operation at lower layer first
and on success from lower layer update the cache for the effected elements.
Delete the cache to session end.

### Cons

1. Lot of string processing required and hence the solution will be time taking.
2. Due to the cache mechanism it will increase memory utilization.

### Effort

High


## Proposal 3

A json file will be introduced which will contain the DM path of all transient
natured objects.
At the time of session start icwmp will perform GPN for all DM objects declared
in the json file and store the results in cache.

When GPN request will come from ACS, icwmp will first perform the operation at
lower layer. Upon receiving the response from lower layer if there exist any
transient DM parameter then it will validate that path against the cache. If
that instance is not present in the cache then drop that parameter from final
result.
If GPN of any sub object of a transient DM object fails at lower layer but the
path exist in the cache then instead of error icwmp will return an empty GPN
response to the ACS but if not exist in cache then will return error.

When GPV request will come from ACS, icwmp will first perform the operation at
lower layer. Upon failure from lower layer icwmp will validate the path in the
cache. If the parameter exist in the cache it will return GPV response with
empty value otherwise will return error.

Delete the cache at session end.

### Cons

1. String processing is required here too but will be optimized by caching only
the transient natured DM parameters. Still it will increase the operation time
to some extend.
2. Memory utilization will also increase to some extend.
3. In some cases it may return empty value for some parameter's GPV request and
which may not be a correct value for e.g such parameters which can have value in
 specific range or specific enum types.

### Effort

Medium

## Proposal 4

Introduce a new flag in bbfdm that allows for the refresh of all transient objects
during a CWMP session. This feature can be enabled or disabled at compile time
based on the customer's preference.

### Cons

1. Increased memory usage due to the need to store all transient objects in internal lists.
2. No updates will occur for transient objects while the CWMP session is active.

### Effort

Low

