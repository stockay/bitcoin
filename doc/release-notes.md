(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Bitcoin Knots version *0.15.0.knots2017FIXME* is now available from:

  <https://bitcoinknots.org/files/0.15.x/0.15.0.knots2017FIXME/>

This is a new major version release, including new features, various bugfixes
and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/bitcoinknots/bitcoin/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the 
installer (on Windows) or just copy over /Applications/Bitcoin-Qt (on Mac)
or `bitcoind`/`bitcoin-qt` (on Linux).

The first time you run version 0.15.0, your chainstate database will be converted to a
new format, which will take anywhere from a few minutes to half an hour,
depending on the speed of your machine.

Note that the block database format also changed in version 0.8.0 and there is no
automatic upgrade code from before version 0.8 to version 0.15.0. Upgrading
directly from 0.7.x and earlier without redownloading the blockchain is not supported.
However, as usual, old wallet versions are still supported.

Downgrading warning
-------------------

The chainstate database for this release is not compatible with previous
releases, so if you run 0.15 and then decide to switch back to any
older version, you will need to run the old release with the `-reindex-chainstate`
option to rebuild the chainstate data structures in the old format.

If your node has pruning enabled, this will entail re-downloading and
processing the entire blockchain.

Compatibility
==============

Bitcoin Knots is supported on multiple operating systems using the Linux kernel,
macOS 10.8+, and Windows Vista and later. Windows XP is not supported.

Bitcoin Knots should also work on most other Unix-like systems but is not
frequently tested on them.

Notable changes
===============

Performance Improvements
------------------------

Version 0.15 contains a number of significant performance improvements, which make
Initial Block Download, startup, transaction and block validation much faster:

- The chainstate database (which is used for tracking UTXOs) has been changed
  from a per-transaction model to a per-output model (See [PR 10195](https://github.com/bitcoin/bitcoin/pull/10195)). Advantages of this model
  are that it:
    - avoids the CPU overhead of deserializing and serializing the unused outputs;
    - has more predictable memory usage;
    - uses simpler code;
    - is adaptable to various future cache flushing strategies.

  As a result, validating the blockchain during Initial Block Download (IBD) and reindex
  is ~30-40% faster, uses 10-20% less memory, and flushes to disk far less frequently.
  The only downside is that the on-disk database is 15% larger. During the conversion from the previous format
  a few extra gigabytes may be used.
- Earlier versions experienced a spike in memory usage while flushing UTXO updates to disk.
  As a result, only half of the available memory was actually used as cache, and the other half was
  reserved to accommodate flushing. This is no longer the case (See [PR 10148](https://github.com/bitcoin/bitcoin/pull/10148)), and the entirety of
  the available cache (see `-dbcache`) is now actually used as cache. This reduces the flushing
  frequency by a factor 2 or more.
- In previous versions, signature validation for transactions has been cached when the
  transaction is accepted to the mempool. Version 0.15 extends this to cache the entire script
  validity (See [PR 10192](https://github.com/bitcoin/bitcoin/pull/10192)). This means that if a transaction in a block has already been accepted to the
  mempool, the scriptSig does not need to be re-evaluated. Empirical tests show that
  this results in new block validation being 40-50% faster.
- LevelDB has been upgraded to version 1.20 (See [PR 10544](https://github.com/bitcoin/bitcoin/pull/10544)). This version contains hardware acceleration for CRC
  on architectures supporting SSE 4.2. As a result, synchronization and block validation are now faster.
- SHA256 hashing has been optimized for architectures supporting SSE 4 (See [PR 10182](https://github.com/bitcoin/bitcoin/pull/10182)). SHA256 is around
  50% faster on supported hardware, which results in around 5% faster IBD and block
  validation. In version 0.15, SHA256 hardware optimization is disabled in release builds by
  default, but can be enabled by using `--enable-experimental-asm` when building.
- Refill of the keypool no longer flushes the wallet between each key which resulted in a ~20x speedup in creating a new wallet. Part of this speedup was used to increase the default keypool to 1000 keys to make recovery more robust. (See [PR 10831](https://github.com/bitcoin/bitcoin/pull/10831)).

Fee Estimation Improvements
---------------------------

Fee estimation has been significantly improved in version 0.15, with more accurate fee estimates used by the wallet and a wider range of options for advanced users of the `estimatesmartfee` and `estimaterawfee` RPCs (See [PR 10199](https://github.com/bitcoin/bitcoin/pull/10199)).

### Changes to internal logic and wallet behavior

- Internally, estimates are now tracked on 3 different time horizons. This allows for longer targets and means estimates adjust more quickly to changes in conditions.
- Estimates can now be *conservative* or *economical*. *Conservative* estimates use longer time horizons to produce an estimate which is less susceptible to rapid changes in fee conditions. *Economical* estimates use shorter time horizons and will be more affected by short-term changes in fee conditions. Economical estimates may be considerably lower during periods of low transaction activity (for example over weekends), but may result in transactions being unconfirmed if prevailing fees increase rapidly.
- By default, the wallet will use conservative fee estimates to increase the reliability of transactions being confirmed within the desired target. For transactions that are marked as replaceable, the wallet will use an economical estimate by default, since the fee can be 'bumped' if the fee conditions change rapidly (See [PR 10589](https://github.com/bitcoin/bitcoin/pull/10589)).
- Estimates can now be made for confirmation targets up to 1008 blocks (one week).
- More data on historical fee rates is stored, leading to more precise fee estimates.
- Transactions which leave the mempool due to eviction or other non-confirmed reasons are now taken into account by the fee estimation logic, leading to more accurate fee estimates.
- The fee estimation logic will make sure enough data has been gathered to return a meaningful estimate. If there is insufficient data, a fallback default fee is used.

### Changes to fee estimate RPCs

- The `estimatefee` RPC is now deprecated in favor of using only `estimatesmartfee` (which is the implementation used by the GUI)
- The `estimatesmartfee` RPC interface has been changed (See [PR 10707](https://github.com/bitcoin/bitcoin/pull/10707)):
    - The `nblocks` argument has been renamed to `conf_target` (to be consistent with other RPC methods).
    - An `estimate_mode` argument has been added. This argument takes one of the following strings: `CONSERVATIVE`, `ECONOMICAL` or `UNSET` (which defaults to `CONSERVATIVE`).
    - The RPC return object now contains an `errors` member, which returns errors encountered during processing.
    - If Bitcoin Knots has not been running for long enough and has not seen enough blocks or transactions to produce an accurate fee estimation, an error will be returned (previously a value of -1 was used to indicate an error, which could be confused for a feerate).
- A new `estimaterawfee` RPC is added to provide raw fee data. External clients can query and use this data in their own fee estimation logic.

Multi-wallet endpoint support
-----------------------------

Bitcoin Knots has supported loading multiple, separate wallets since
v0.13.1.knots20161027. However, due to the RPC API, RPC users could only access
a single wallet at a time - accessing multiple wallets over RPC required the
use of multiple RPC users. 0.15.0 adds support for specifying the wallet by
name using a URI path for the RPC endpoint.

To do this, HTTP RPC requests should be send to the `<RPC IP address>:<RPC port>/wallet/<wallet name>/` endpoint, for example `127.0.0.1:8332/wallet/wallet1.dat/`.

`bitcoin-cli` commands should be run with a `-rpcwallet` option, for example `bitcoin-cli -rpcwallet=wallet1.dat getbalance`.

Additionally, a new `listwallets` RPC method is added to display which wallets are currently loaded. The names returned by this method are the same as those used in the HTTP endpoint and for the `rpcwallet` argument.

This new wallet endpoint interface should be considered unstable for version 0.15.0, and there may backwards-incompatible changes in future versions.

The original RPC user based wallet selection continues to work as well, independently from the new endpoint support. `rpcauth` users that specify a specific wallet are prohibited from using endpoints to access others. However, note that (at least in this version) `listwallets` will always list all wallets on the node, even if access to them has been limited!

Removal of no-fee transactions
------------------------------

In previous versions of Bitcoin Knots, transactions without a fee would be allowed to be relayed if the node's memory pool was empty and the age and value of UTXOs they spent (coin age priority) was sufficiently high.

For a long time now, this has been effectively useless, due to wide adoption of Bitcoin as well as regular spam attacks. Since the code no longer has a purpose, support for no-fee transactions has been removed.

Mempool Persistence Across Restarts
-----------------------------------

Version 0.13.1.knots20161212 introduced mempool persistence across restarts (the mempool is saved to a `mempool.dat` file in the data directory prior to shutdown and restores the mempool when the node is restarted). Version 0.15 allows this feature to be switched on or off using the `-persistmempool` command-line option (See [PR 9966](https://github.com/bitcoin/bitcoin/pull/9966)). By default, the option is set to true, and the mempool is saved on shutdown and reloaded on startup. If set to false, the `mempool.dat` file will neither be loaded on startup nor saved on shutdown.

New RPC methods
---------------

Version 0.15 introduces several new RPC methods:

- `abortrescan` stops current wallet rescan, e.g. when triggered by an `importprivkey` call (See [PR 10208](https://github.com/bitcoin/bitcoin/pull/10208)).
- `combinerawtransaction` accepts a JSON array of raw transactions and combines them into a single raw transaction (See [PR 10571](https://github.com/bitcoin/bitcoin/pull/10571)).
- `estimaterawfee` returns raw fee data so that customized logic can be implemented to analyze the data and calculate estimates. See [Fee Estimation Improvements](#fee-estimation-improvements) for full details on changes to the fee estimation logic and interface.
- `getchaintxstats` returns statistics about the total number and rate of transactions
  in the chain (See [PR 9733](https://github.com/bitcoin/bitcoin/pull/9733)).
- `listwallets` lists wallets which are currently loaded. See the *Multi-wallet* section
  of these release notes for full details (See [Multi-wallet endpoint support](#multi-wallet-endpoint-support)).
- `uptime` returns the total runtime of the `bitcoind` server since its last start (See [PR 10400](https://github.com/bitcoin/bitcoin/pull/10400)).

Low-level RPC changes
---------------------

- When using Bitcoin Knots in multi-wallet mode, RPC requests for wallet methods may specify
  the wallet that they're intended for. See [Multi-wallet endpoint support](#multi-wallet-endpoint-support) for full details.

- The `optintorbf` parameter for `createrawtransaction` and `fundrawtransaction` has been renamed to `replaceable` to match `bumpfee`.

- The new database model no longer stores information about transaction
  versions of unspent outputs (See [Performance improvements](#performance-improvements)). This means that:
  - The `gettxout` RPC no longer has a `version` field in the response.
  - The `gettxoutsetinfo` RPC reports `hash_serialized_2` instead of `hash_serialized`,
    which does not commit to the transaction versions of unspent outputs, but does
    commit to the height and coinbase information.
  - The `getutxos` REST path no longer reports the `txvers` field in JSON format,
    and always reports 0 for transaction versions in the binary format

- The `estimatefee` RPC is deprecated. Clients should switch to using the `estimatesmartfee` RPC, which returns better fee estimates. See [Fee Estimation Improvements](#fee-estimation-improvements) for full details on changes to the fee estimation logic and interface.

- The `gettxoutsetinfo` response now contains `disk_size` and `bogosize` instead of
  `bytes_serialized`. The first is a more accurate estimate of actual disk usage, but
  is not deterministic. The second is unrelated to disk usage, but is a
  database-independent metric of UTXO set size: it counts every UTXO entry as 50 + the
  length of its scriptPubKey (See [PR 10426](https://github.com/bitcoin/bitcoin/pull/10426)).

- `signrawtransaction` can no longer be used to combine multiple transactions into a single transaction. Instead, use the new `combinerawtransaction` RPC (See [PR 10571](https://github.com/bitcoin/bitcoin/pull/10571)).

- `fundrawtransaction` no longer accepts a `reserveChangeKey` option. This option used to allow RPC users to fund a raw transaction using an key from the keypool for the change address without removing it from the available keys in the keypool. The key could then be re-used for a `getnewaddress` call, which could potentially result in confusing or dangerous behaviour (See [PR 10784](https://github.com/bitcoin/bitcoin/pull/10784)).

- `estimatepriority` and `estimatesmartpriority` have been removed. Estimating coin age priority is no longer supported.

- In the `getpeerinfo` RPC, the return object for each peer now returns an `addrbind` member, which contains the ip address and port of the connection to the peer. This is in addition to the `addrlocal` member which contains the ip address and port of the local node as reported by the peer (See [PR 10478](https://github.com/bitcoin/bitcoin/pull/10478)).

- The `resendwallettransactions` RPC throws an error if the `-walletbroadcast` option is set to false (See [PR 10995](https://github.com/bitcoin/bitcoin/pull/10995)).

- The second argument in the `submitblock` RPC argument has been renamed from `parameters` to `dummy`. This argument never had any effect, and the renaming is simply to communicate this fact to the user (See [PR 10191](https://github.com/bitcoin/bitcoin/pull/10191))
  (Clients should, however, use positional arguments for `submitblock` in order to be compatible with BIP 22.)

Low-level p2p changes
---------------------

TODO: MSG_FILTERED_WITNESS_BLOCK

0.15.0 Change log
=================

...

AcceptToMemoryPool: Replace fLimitFree[=false] with rejectmsg_gratis in ignore_rejects
AcceptToMemoryPool: Standardise rejection reason format
Add a new checkpoint at block 481,824
ArgsManager: ForceSetArg with int64_t
Bugfix: Use testnet RequireStandard for -acceptnonstdtxn default
Bump minimum Windows version to Vista
CTxMemPool::check: Grab chainActive.Height() internally
CValidationInterface: ValidationInterfaceUnregistering, called when being unregistered
Handle MSG_FILTERED_WITNESS_BLOCK messages
Move Win32 defines to configure.ac to ensure they are globally defined
Move coin-age priority logic to its own file
QA: Functional test for sweepprivkeys
QA: prioritise_transaction: Reduce unnecessary change in variable names
RPC: prioritisetransaction: Make both deltas optional
Trivial: Replace AllowFree with MINIMUM_TX_PRIORITY
coin_age_priority: Ensure nPackagesSelected and nDescendantsUpdated get updated for priority-mined transactions
coin_age_priority: Move priority calculation code off non-policy objects
coin_age_priority: Refactor TestForBlock to use TestPackage & TestPackageTransactions
utilioprio: Add Windows support as ioprio_set_file_idle
wallet: Key origin refactoring

Credits
=======

Thanks to everyone who directly contributed to this release:

- ロハン ダル
- Ahmad Kazi
- aideca
- Akio Nakamura
- Alex Morcos
- Allan Doensen
- Andres G. Aragoneses
- Andrew Chow
- Angel Leon
- Awemany
- Bob McElrath
- Brian McMichael
- BtcDrak
- Charlie Lee
- Chris Gavin
- Chris Stewart
- Cory Fields
- CryptAxe
- Dag Robole
- Daniel Aleksandersen
- Daniel Cousens
- darksh1ne
- Dimitris Tsapakidis
- Eric Lombrozo
- Eric Shaw
- Evan Klitzke
- fanquake
- Felix Weis
- flack
- Greg Griffith
- Gregory Maxwell
- Gregory Sanders
- Ian Kelling
- Jack Grigg
- James Evans
- James Hilliard
- Jameson Lopp
- Jeremy Rubin
- Jimmy Song
- João Barbosa
- Johnathan Corgan
- John Newbery
- Jonas Schnelli
- Jorge Timón
- Kalle Alm
- Karl-Johan Alm
- kewde
- KibbledJiveElkZoo
- Kirit Thadaka
- kobake
- Kyle Honeycutt
- Lawrence Nahum
- Luke Dashjr
- Marco Falke
- Marcos Mayorga
- Marijn Stollenga
- Mario Dian
- Mark Friedenbach
- Marko Bencun
- Masahiko Hyuga
- Matt Corallo
- Matthew Zipkin
- Matthias Grundmann
- Michael Goldstein
- Michael Rotarius
- Mikerah
- Mike van Rossum
- Mitchell Cash
- Nicolas Dorier
- Patrick Strateman
- Pavel Janík
- Pavlos Antoniou
- Pavol Rusnak
- Pedro Branco
- Peter Todd
- Pieter Wuille
- practicalswift
- René Nyffenegger
- Ricardo Velhote
- romanornr
- Russell Yanofsky
- Rusty Russell
- Ryan Havar
- shaolinfry
- Shigeya Suzuki
- Simone Madeo
- Spencer Lievens
- Steven D. Lander
- Suhas Daftuar
- Takashi Mitsuta
- Thomas Snider
- Timothy Redaelli
- tintinweb
- tnaka
- Warren Togami
- Wladimir J. van der Laan

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).
