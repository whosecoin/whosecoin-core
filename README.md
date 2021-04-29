## Binary Tuple Representation
We define a specific binary format for tuples of arbitrary type by the following grammar.
```
tuple   ::= "(" e_list ")"

e_list  ::= element e_list
          | ""

element ::= tuple           // nested tuple
          | "i" int32_t     // big-endian 32-bit signed integer
          | "I" int64_t     // big-endian 64-bit signed integer
          | "u" uint32_t    // big-endian 32-bit unsigned integer
          | "U" uint64_t    // big-endian 64-bit unsigned integer
          | "f" float       // IEEE 754 single-precison float
          | "F" double      // IEEE 754 double-precison float
          | "b" bool        // 0x01 if true and 0x00 if false
          | "B" buffer      // arbitrary binary data
          | "s" string      // 0x00 terminated string
          | "n"             // null

buffer  ::= uint32_t uint8_t[]
```

## Transactions
We define a transaction t as a 3-tuple (s, r, v, n) where s is a 256-bit buffer containing the Ed25519 public key of the sender, r is a 256-bit buffer containing the Ed25519 public key of the recipient, v is an unsigned 64 bit integer indicating the value being transfered, and n is an arbitrary unsigned 32 bit integer that distinguishes otherwise identical transactions. 

## Transaction Hash
We define the transaction hash of t as the 256-bit BLAKE2b cryptographic hash of the binary tuple representation of t.

## Signed Transactions
We define a signed transaction as a 2-tuple (t, s) where t is a transaction and s is a 512-byte buffer containing the sender's Ed25519 cryptographic signature of the binary tuple representation of t.

## Signed Transaction List
We define a list of k signed transactions ts1, ts2, ..., tsk as a k-tuple (ts1, ts2, ..., tsk).

## Merkle Root
For any signed transaction list (ts1, ts2, ..., tsk), we define the merkle root m as the result of recursively hashing together the transaction hashes of adjacent pairs of transactions. For example: the merkle root of the transaction list (ts1, ts2, ts3, ts4, ts5) is H(H(H(H(t1), H(t2)), H(H(t3), H(t4))), H(t4)) where H is the BLAKE2b hash function. The merkle root can be used to uniquely identify transaction lists and efficiently prove list membership with limited information.

## Block
We define a block as a 2-tuple (h, l) where h is a block header (defined below) and l is a signed transaction list. We define the block hash as the BLAKE2b hash of the block header h. This hash uniquely identifies a block.

## Block Header
We define a block header as a 6-tuple (t, p, m, k, d, n) where t is the unsigned 64-bit value containing the unix time when the block was create, p is the 256-bit buffer containing the block hash of the previous block, m is the 256-bit buffer containing the merkle root of the transaction list, k the the length of the transaction list, d is a unsigned 32 bit integer difficulty of the block, and n is an arbitrary unsigned 32 bit integer. Note that if there is no previous block, p is a 256-bit buffer containing all zeros. If there are no transactions in the transaction list, then m is a 256-bit buffer containing all zeros.

## Block Difficulty
For a block of difficulty d to be considered valid, its hash must start with at least d zeros. For a random oracle hash function, the probability of block b starting with at least d zeros is (1/2)^d. Since changing the nonce is sufficient to change the hash of b, a block can be _mined_ by iterating through different nonce and time values until a valid block is found. The expected number of hashes required to find a valid block follows a geometric distribution with value 2^d. We will call this value the block weight.

## Block Tree
Since each block contains the hash of the previous block, a set of blocks form an implicit tree structure. 

## Blockchain
We denote each branch of a block tree as a blockchain.

## Blockchain Weight
For each blockchain b1 <- b2 <- ... <- bk, we define the cumulative weight as the sum of the weight of every block in the chain. 

## Principal Blockchain
We define the principal blockchain as the blockchain with the largest cumulative weight. In the case that there are two blockchains each with the same cumulative weight, the principal blockchain is the blockchain that was created first.

## Game Theoretic Analysis
Consider an infinite time horizon game with exponential discounting in which n players compete to construct a blockchain. At each discrete timestep, all players act simultaneously to optionally "mine" a block by attempting to extend any branch in the block tree (-1 utility). At each timestep, exactly one agent will be randomly chosen to create their block and recieve (+1 token) on their chosen branch of the block tree. At any timestep, a player can also choose to exchange (-k tokens) from the principal block chain for (+pk utility) where p is the fixed price of token. Additionally, we enforce a rule that tokens can only be exchanged for utility if they are on the principal block chain at the time of exchange and that they are at least m blocks from the leaf node.

A subgame perfect Nash equilibrium is where all players mine the principal blockchain of the previous timestep and exchange them for utility as soon as possible. This can be proven with the one-shot deviation principal. Since the agents are future discounting, they would strictly prefer to have utility at an earlier time-step than a later time-step, so they always exchange tokens for utility as soon as possible. In equilibrium, a player than mines on the principal blockchain recieves an expected (1/n) tokens on the principal blockchain. After m time-steps, they recieve an expected p*(1/n) utility. If any agent attempts to deviate from the strategy by mining a block other than the principal blockchain, they must spend 1 utility to do so. The only way to recoup this cost is to continue mining on their blockchain until they overtake the principal blockchain. 

Consider any subgame in which there exists a branch of the block tree that is the same weight as the principal block chain. If an agent has more tokens on the non-principal blockchain, the agent would strictly benefit from mining the non-principal blockchain since it would become the principal blockchain in the next time-step.

Consider any subgame in which there exists a branch of the block tree that has a weight that is exactly 1 less than the principal block chain. The probability of mining two blocks in a row is (1/n)^2. With probability (n-1)/n the agent will be two blocks behind after the next time-step. If an agent has more tokens on the non-principal blockchain, the agent would strictly benefit from mining the non-principal blockchain since it would become the principal blockchain in the next time-step.