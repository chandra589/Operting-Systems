/*
 * MTF: Encode/decode using "move-to-front" transform and Fibonacci encoding.
 */

#include <stdlib.h>
#include <stdio.h>

#include "mtft.h"
#include "debug.h"
#include "helper_Funcs.h"

/**
 * @brief  Given a symbol value, determine its encoding (i.e. its current rank).
 * @details  Given a symbol value, determine its encoding (i.e. its current rank)
 * according to the "move-to-front" transform described in the assignment
 * handout, and update the state of the "move-to-front" data structures
 * appropriately.
 *
 * @param sym  An integer value, which is assumed to be in the range
 * [0, 255], [0, 65535], depending on whether the
 * value of the BYTES field in the global_options variable is 1 or 2.
 *
 * @return  An integer value in the range [0, 511] or [0, 131071]
 * (depending on the value of the BYTES field in the global_options variable),
 * which is the rank currently assigned to the argument symbol.
 * The first time a symbol is encountered, it is assigned
 * a default rank computed by adding 512 or 131072 to its value.
 * A symbol that has already been encountered is assigned a rank in the
 * range [0, 255], [0, 65535], according to how recently it has occurred
 * in the input compared to other symbols that have already been seen.
 * For example, if this function is called twice in succession
 * with the same value for the sym parameter, then the second time the
 * function is called the value returned will be 0 because the symbol will
 * have been "moved to the front" on the previous call.
 *
 * @modifies  The state of the "move-to-front" data structures.
 */
CODE mtf_map_encode(SYMBOL sym) {

    //debug("start of encode, current_offset: %ld\n", current_offset);
    int depth = 0, IsOddValue = 0, codeVal = 0;
    int _ispoweroftwo;
    if (current_offset == 0)
        _ispoweroftwo = 1;
    else if (current_offset == 1)
        _ispoweroftwo = 0;
    else
        _ispoweroftwo = IsPowerofTwo(current_offset, &depth, &IsOddValue);

    //debug("Is power of 2- true(1), false(0) & depth: %d %d\n", _ispoweroftwo, depth);
    if (_ispoweroftwo)
    {
        MTF_NODE* temp = mtf_map;
        if (recycled_node_list)
        {
            mtf_map = recycled_node_list;
            recycled_node_list = recycled_node_list->left_child;
            mtf_map->left_child = NULL;
            mtf_map->right_child = NULL;
            mtf_map->left_count = 0;
            mtf_map->right_count = 0;
        }
        else
        {
            mtf_map = node_pool + first_unused_node_index;
            first_unused_node_index++;
        }
        mtf_map->left_child = temp;
        temp->parent = mtf_map;
        mtf_map->right_child = NULL;
        mtf_map->left_count = temp->left_count + temp->right_count;
        mtf_map->right_count = 0;
    }
    if (last_offset[sym] != 0)
    {
        //debug("symbol value in tree\n");
        long lastoffsetVal = last_offset[sym];
        MTF_NODE* leafnode = mtf_map;
        //debug("offset value present in array, depth, head pointer: %ld %d %p\n", lastoffsetVal, depth, leafnode);

        for (int i = 0; i < (depth + 1); i++)
        {
            if ((lastoffsetVal - 1) & (1 << (depth - i)))
            {
                leafnode = leafnode->right_child;
                //debug("right child poiter: %p\n", leafnode);
            }
            else
            {
                leafnode = leafnode->left_child;
                //debug("left child pointer: %p\n", leafnode);
            }
        }

        //debug("symbol present in node: %d\n", leafnode->symbol);

        if (leafnode->symbol == sym) //should match, otherwise algo was wrong
        {
            //debug("symbols matched\n");
            //go back, to find the rank and remove the unnecessary nodes and add them to recycled nodes
            MTF_NODE* parentnode = leafnode->parent;
            MTF_NODE* childnode = leafnode;
            while (parentnode != NULL)
            {
                //debug("parent node and child node: %p %p\n", parentnode, childnode);
                if (childnode->left_child == NULL && childnode->right_child == NULL)
                {
                    //then remove child from parent
                    childnode->left_count = childnode->right_count = 0;
                    childnode->parent = NULL;
                    if (recycled_node_list)
                    {
                        childnode->left_child = recycled_node_list;
                        recycled_node_list = childnode;
                    }
                    else
                        recycled_node_list = childnode;

                    //debug("pn lchild, pn rchild, child-node: %p %p %p\n", parentnode->left_child, parentnode->right_child, childnode);

                    if (parentnode->left_child == childnode)
                    {
                        //debug("pn lc: %d\n", parentnode->left_count);
                        parentnode->left_count--;
                        codeVal += parentnode->right_count;
                        parentnode->left_child = NULL;
                    }
                    else
                    {
                        //debug("pn rc: %d\n", parentnode->right_count);
                        parentnode->right_count--;
                        parentnode->right_child = NULL;
                    }
                }
                else
                {
                    //no need to remove child node
                    if (parentnode->left_child == childnode)
                    {
                        //debug("pn lc: %d\n", parentnode->left_count);
                        parentnode->left_count--;
                        codeVal += parentnode->right_count;
                    }
                    else
                    {
                        //debug("pn rc: %d\n", parentnode->right_count);
                        parentnode->right_count--;
                    }
                }

                childnode = parentnode;
                parentnode = parentnode->parent;
            }

            //insert the symbol in the current offset
            MTF_NODE* newnode = NULL;
            MTF_NODE* temp = mtf_map;
            for (int i = 0; i < depth; i++)
            {
                if (current_offset & (1 << (depth - i)))
                {
                    temp->right_count++;
                    if (temp->right_child)
                        temp = temp->right_child;
                    else
                    {
                        //get a new node
                        MTF_NODE* createnode;
                        if (recycled_node_list)
                        {
                            //then take from list
                            createnode = recycled_node_list;
                            recycled_node_list = recycled_node_list->left_child;
                            createnode->left_child = NULL;
                            createnode->right_child = NULL;
                            createnode->left_count = 0;
                            createnode->right_count = 0;
                        }
                        else
                        {
                            createnode = node_pool + first_unused_node_index;
                            first_unused_node_index++;
                        }
                        temp->right_child = createnode;
                        createnode->parent = temp;
                        temp = temp->right_child;
                    }
                }
                else
                {
                    temp->left_count++;
                    if (temp->left_child)
                        temp = temp->left_child;
                    else
                    {
                        //get a new node
                        MTF_NODE* createnode;
                        if (recycled_node_list)
                        {
                            //then take from list
                            createnode = recycled_node_list;
                            recycled_node_list = recycled_node_list->left_child;
                            createnode->left_child = NULL;
                            createnode->right_child = NULL;
                            createnode->left_count = 0;
                            createnode->right_count = 0;
                        }
                        else
                        {
                            createnode = node_pool + first_unused_node_index;
                            first_unused_node_index++;
                        }
                        temp->left_child = createnode;
                        createnode->parent = temp;
                        temp = temp->left_child;
                    }
                }
            }


            if (recycled_node_list)
            {
                //then take from list
                newnode = recycled_node_list;
                recycled_node_list = recycled_node_list->left_child;
                newnode->left_child = NULL;
                newnode->right_child = NULL;
                newnode->left_count = 0;
                newnode->right_count = 0;
            }
            else
            {
                newnode = node_pool + first_unused_node_index;
                first_unused_node_index++;
            }

            if (current_offset & 1)
            {
                temp->right_count++;
                temp->right_child = newnode;
            }
            else
            {
                temp->left_count++;
                temp->left_child = newnode;
            }
            newnode->symbol = sym;
            newnode->parent = temp;
            current_offset++;
            last_offset[sym] = current_offset;
            return codeVal;
        }

    }
    else if (last_offset[sym] == 0)
    {
        //debug("symbol not found - Inserting new node\n");
        MTF_NODE* temp = mtf_map;
        for (int i = 0; i < depth; i++)
        {
            if (current_offset & (1 << (depth - i)))
            {
                temp->right_count++;
                if (temp->right_child)
                    temp = temp->right_child;
                else
                {
                    //have to keep a new node
                    MTF_NODE* createnode;
                    if (recycled_node_list)
                    {
                        createnode = recycled_node_list;
                        recycled_node_list = recycled_node_list->left_child;
                        createnode->right_child = NULL;
                        createnode->left_child = NULL;
                        createnode->left_count = 0;
                        createnode->right_count = 0;
                    }
                    else
                    {
                        createnode = node_pool + first_unused_node_index;
                        first_unused_node_index++;
                    }
                    temp->right_child = createnode;
                    createnode->parent = temp;
                    temp = temp->right_child;
                }
            }
            else
            {
                temp->left_count++;
                if (temp->left_child)
                    temp = temp->left_child;
                else
                {
                    MTF_NODE* createnode;
                    if (recycled_node_list)
                    {
                        createnode = recycled_node_list;
                        recycled_node_list = recycled_node_list->left_child;
                        createnode->right_child = NULL;
                        createnode->left_child = NULL;
                        createnode->left_count = 0;
                        createnode->right_count = 0;
                    }
                    else
                    {
                        createnode = node_pool + first_unused_node_index;
                        first_unused_node_index++;
                    }
                    temp->left_child = createnode;
                    createnode->parent = temp;
                    temp = temp->left_child;
                }
            }
        }
        //now temp will be at the parent of child node to be inserted.
        //insert a new node to right or left based on least significant bit operation
        //check recycled node list first
        MTF_NODE* newnode = NULL;
        if (recycled_node_list)
        {
            //then take from list
            newnode = recycled_node_list;
            recycled_node_list = recycled_node_list->left_child;
            newnode->left_child = NULL;
            newnode->right_child = NULL;
            newnode->left_count = 0;
            newnode->right_count = 0;
        }
        else
        {
            newnode = node_pool + first_unused_node_index;
            first_unused_node_index++;
        }

        if (current_offset & 1)
        {
            temp->right_count++;
            temp->right_child = newnode;
        }
        else
        {
            temp->left_count++;
            temp->left_child = newnode;
        }
        newnode->parent = temp;
        newnode->symbol = sym;
        current_offset++;
        last_offset[sym] = current_offset;
        if (global_options & 1)
            return (sym + 256);
        else
            return (sym + 65536);
    }

    return NO_SYMBOL;
}


/**
 * @brief Given an integer code, return the symbol currently having that code.
 * @details  Given an integer code, interpret the code as a rank, find the symbol
 * currently having that rank according to the "move-to-front" transform
 * described in the assignment handout, and update the state of the
 * "move-to-front" data structures appropriately.
 *
 * @param code  An integer value, which is assumed to be in the range
 * [0, 511] or [0, 131071], depending on the value of the BYTES field in
 * the global_options variable.
 *
 * @return  An integer value in the range [0, 255] or [0, 65535]
 * (depending on value of the BYTES field in the global_options variable),
 * which is the symbol having the specified argument value as its current rank.
 * Argument values in the upper halves of the respective input ranges will be
 * regarded as the default ranks of symbols that have not yet been encountered,
 * and the corresponding symbol value will be determined by subtracting 256 or
 * 65536, respectively.  Argument values in the lower halves of the respective
 * input ranges will be regarded as the current ranks of symbols that
 * have already been seen, and the corresponding symbol value will be
 * determined in accordance with the move-to-front transform.
 *
 * @modifies  The state of the "move-to-front" data structures.
 */
SYMBOL mtf_map_decode(CODE code) {

    //debug("start of decode 2, current_offset: %ld\n", current_offset);
    //debug("code value: %d\n", code);
    int depth = 0, IsOddValue = 0;
    int _ispoweroftwo;
    if (current_offset == 0)
        _ispoweroftwo = 1;
    else if (current_offset == 1)
        _ispoweroftwo = 0;
    else
        _ispoweroftwo = IsPowerofTwo(current_offset, &depth, &IsOddValue);

    //debug("Is power of 2- true(1), false(0) & depth: %d %d\n", _ispoweroftwo, depth);
    if (_ispoweroftwo)
    {
        MTF_NODE* temp = mtf_map;
        if (recycled_node_list)
        {
            mtf_map = recycled_node_list;
            recycled_node_list = recycled_node_list->left_child;
            mtf_map->left_child = NULL;
            mtf_map->right_child = NULL;
            mtf_map->left_count = 0;
            mtf_map->right_count = 0;
        }
        else
        {
            mtf_map = node_pool + first_unused_node_index;
            first_unused_node_index++;
        }
        mtf_map->left_child = temp;
        temp->parent = mtf_map;
        mtf_map->right_child = NULL;
        mtf_map->left_count = temp->left_count + temp->right_count;
        mtf_map->right_count = 0;
    }

    //just Insert (true/false)
    //if _justInsert is true, then symbol not present in tree.
    int _justInsert = 0;
    if (global_options & 1)
    {
        if (code >= 256)
            _justInsert = 1;
    }
    else
    {
        if (code >= 65536)
            _justInsert = 1;
    }

    if (_justInsert == 0)
    {
        //debug("symbol in tree\n");
        MTF_NODE* leafnode = mtf_map;
        //debug("Depth & head pointer: %d %p\n", depth, leafnode);
        //debug("left_child, right_child, left_count, right_count: %p %p %d %d", leafnode->left_child, leafnode->right_child, leafnode->left_count, leafnode->right_count);

        //based on left count and right count at each node, have to reach the corresponsing code
        int temp_cvalue = code;

        for (int i = 0; i < (depth+1); i++)
        {
            if (temp_cvalue != 0)
            {
                if (temp_cvalue >= leafnode->right_count)
                {
                    //have to go left
                    temp_cvalue = temp_cvalue - leafnode->right_count;
                    //debug("updated c value: %d\n", temp_cvalue);
                    MTF_NODE* l_child = NULL;
                    l_child = leafnode->left_child;
                    if (l_child)
                        leafnode = leafnode->left_child;
                    else
                        leafnode = leafnode->right_child;
                    //debug("left_child poiter: %p\n", leafnode);
                    //debug("left_child, right_child, left_count, right_count: %p %p %d %d", leafnode->left_child, leafnode->right_child, leafnode->left_count, leafnode->right_count);
                }
                else
                {
                    //have to go right
                    MTF_NODE* r_child = NULL;
                    r_child = leafnode->right_child;
                    if (r_child)
                        leafnode = leafnode->right_child;
                    else
                        leafnode = leafnode->left_child;
                    //debug("right_child pointer: %p\n", leafnode);
                    //debug("left_child, right_child, left_count, right_count: %p %p %d %d", leafnode->left_child, leafnode->right_child, leafnode->left_count, leafnode->right_count);
                }

                if (leafnode == NULL)
                    return NO_SYMBOL;
            }
            else
            {
                //it has to go right if right node is present, otherwise left
                MTF_NODE* r_child = NULL;
                r_child = leafnode->right_child;
                if (r_child)
                    leafnode = leafnode->right_child;
                else
                    leafnode = leafnode->left_child;

                if (leafnode == NULL)
                    return NO_SYMBOL;
            }
        }

        //debug("symbol we are looking for: %d\n", leafnode->symbol);
        int sym = leafnode->symbol;

        //go back, to find the rank and remove the unnecessary nodes and add them to recycled nodes
        MTF_NODE* parentnode = leafnode->parent;
        MTF_NODE* childnode = leafnode;
        while (parentnode != NULL)
        {
            //debug("parent node and child node: %p %p\n", parentnode, childnode);
            if (childnode->left_child == NULL && childnode->right_child == NULL)
            {
                //then remove child from parent
                childnode->left_count = 0, childnode->right_count = 0;
                childnode->parent = NULL;
                if (recycled_node_list)
                {
                    childnode->left_child = recycled_node_list;
                    recycled_node_list = childnode;
                }
                else
                    recycled_node_list = childnode;

                //debug("pn lchild, pn rchild, child-node: %p %p %p\n", parentnode->left_child, parentnode->right_child, childnode);

                if (parentnode->left_child == childnode)
                {
                    //debug("pn lc: %d\n", parentnode->left_count);
                    parentnode->left_count--;
                    parentnode->left_child = NULL;
                }
                else
                {
                    //debug("pn rc: %d\n", parentnode->right_count);
                    parentnode->right_count--;
                    parentnode->right_child = NULL;
                }
            }
            else
            {
                //no need to remove child node
                if (parentnode->left_child == childnode)
                {
                    //debug("pn lc: %d\n", parentnode->left_count);
                    parentnode->left_count--;
                }
                else
                {
                    //debug("pn rc: %d\n", parentnode->right_count);
                    parentnode->right_count--;
                }
            }

            childnode = parentnode;
            parentnode = parentnode->parent;
        }

        //debug("removed the node and made changes to lc and rc from leaf to head\n");

        //insert the symbol in the current offset
        MTF_NODE* newnode = NULL;
        MTF_NODE* temp = mtf_map;
        for (int i = 0; i < depth; i++)
        {
            if (current_offset & (1 << (depth - i)))
            {
                temp->right_count++;
                if (temp->right_child)
                    temp = temp->right_child;
                else
                {
                    //get a new node
                    MTF_NODE* createnode;
                    if (recycled_node_list)
                    {
                        //then take from list
                        createnode = recycled_node_list;
                        recycled_node_list = recycled_node_list->left_child;
                        createnode->left_child = NULL;
                        createnode->right_child = NULL;
                        createnode->left_count = 0;
                        createnode->right_count = 0;
                    }
                    else
                    {
                        createnode = node_pool + first_unused_node_index;
                        first_unused_node_index++;
                    }
                    temp->right_child = createnode;
                    createnode->parent = temp;
                    temp = temp->right_child;
                }
            }
            else
            {
                temp->left_count++;
                if (temp->left_child)
                    temp = temp->left_child;
                else
                {
                    //get a new node
                    MTF_NODE* createnode;
                    if (recycled_node_list)
                    {
                        //then take from list
                        createnode = recycled_node_list;
                        recycled_node_list = recycled_node_list->left_child;
                        createnode->left_child = NULL;
                        createnode->right_child = NULL;
                        createnode->left_count = 0;
                        createnode->right_count = 0;
                    }
                    else
                    {
                        createnode = node_pool + first_unused_node_index;
                        first_unused_node_index++;
                    }
                    temp->left_child = createnode;
                    createnode->parent = temp;
                    temp = temp->left_child;
                }
            }
        }

        //debug("reached last parent node\n");

        if (recycled_node_list)
        {
            //then take from list
            newnode = recycled_node_list;
            recycled_node_list = recycled_node_list->left_child;
            newnode->left_child = NULL;
            newnode->right_child = NULL;
            newnode->left_count = 0;
            newnode->right_count = 0;
        }
        else
        {
            newnode = node_pool + first_unused_node_index;
            first_unused_node_index++;
        }

        if (current_offset & 1)
        {
            temp->right_count++;
            temp->right_child = newnode;
        }
        else
        {
            temp->left_count++;
            temp->left_child = newnode;
        }
        newnode->symbol = sym;
        newnode->parent = temp;
        current_offset++;
        return sym;
    }

    else if (_justInsert == 1)
    {
        //debug("symbol not found - Inserting new node\n");
        MTF_NODE* temp = mtf_map;
        for (int i = 0; i < depth; i++)
        {
            if (current_offset & (1 << (depth - i)))
            {
                temp->right_count++;
                if (temp->right_child)
                    temp = temp->right_child;
                else
                {
                    //have to keep a new node
                    MTF_NODE* createnode;
                    if (recycled_node_list)
                    {
                        createnode = recycled_node_list;
                        recycled_node_list = recycled_node_list->left_child;
                        createnode->right_child = NULL;
                        createnode->left_child = NULL;
                        createnode->left_count = 0;
                        createnode->right_count = 0;
                    }
                    else
                    {
                        createnode = node_pool + first_unused_node_index;
                        first_unused_node_index++;
                    }
                    temp->right_child = createnode;
                    createnode->parent = temp;
                    temp = temp->right_child;
                }
            }
            else
            {
                temp->left_count++;
                if (temp->left_child)
                    temp = temp->left_child;
                else
                {
                    MTF_NODE* createnode;
                    if (recycled_node_list)
                    {
                        createnode = recycled_node_list;
                        recycled_node_list = recycled_node_list->left_child;
                        createnode->right_child = NULL;
                        createnode->left_child = NULL;
                        createnode->left_count = 0;
                        createnode->right_count = 0;
                    }
                    else
                    {
                        createnode = node_pool + first_unused_node_index;
                        first_unused_node_index++;
                    }
                    temp->left_child = createnode;
                    createnode->parent = temp;
                    temp = temp->left_child;
                }
            }
        }
        //now temp will be at the parent of child node to be inserted.
        //insert a new node to right or left based on least significant bit operation
        //check recycled node list first
        MTF_NODE* newnode = NULL;
        if (recycled_node_list)
        {
            //then take from list
            newnode = recycled_node_list;
            recycled_node_list = recycled_node_list->left_child;
            newnode->left_child = NULL;
            newnode->right_child = NULL;
            newnode->left_count = 0;
            newnode->right_count = 0;
        }
        else
        {
            newnode = node_pool + first_unused_node_index;
            first_unused_node_index++;
        }

        if (current_offset & 1)
        {
            temp->right_count++;
            temp->right_child = newnode;
        }
        else
        {
            temp->left_count++;
            temp->left_child = newnode;
        }
        newnode->parent = temp;
        newnode->symbol = code;
        current_offset++;
        if (global_options & 1)
            return (code - 256);
        else
            return (code - 65536);
    }

    return NO_SYMBOL;
}

/**
 * @brief  Perform data compression.
 * @details  Read uncompressed data from stdin and write the corresponding
 * compressed data to stdout, using the "move-to-front" transform and
 * Fibonacci coding, as described in the assignment handout.
 *
 * Data is read byte-by-byte from stdin, and each group of one or two
 * bytes is interpreted as a single "symbol" (according to whether
 * the BYTES field of the global_options variable is 1 or 2).
 * Multi-byte symbols are constructed according to "big-endian" byte order:
 * the first byte read is used as the most-significant byte of the symbol
 * and the last byte becomes the least-significant byte.  The "move-to-front"
 * transform is used to map each symbol read to its current rank.
 * As described in the assignment handout, the range of possible ranks is
 * twice the size of the input alphabet.  For example, 1-byte input symbols
 * have values in the range [0, 255] and their ranks have values in the
 * range [0, 511].  Ranks in the lower range are used for symbols that have
 * already been encountered in the input, and ranks in the upper range
 * serve as default ranks for symbols that have not yet been seen.
 *
 * Once a symbol has been mapped to a rank r, Fibonacci coding is applied
 * to the value r+1 (which will therefore always be a positive integer)
 * to obtain a corresponding code word (conceptually, a sequence of bits).
 * The successive code words obtained as each symbol is read are concatenated
 * and the resulting bit string is blocked into 8-bit bytes (according to
 * "big-endian" bit order).  These 8-bit bytes are written successively to
 * stdout.
 *
 * When the end of input is reached, any partial output byte is "padded"
 * with 0 bits in the least-signficant positions to reach a full 8-bit
 * byte, which is emitted as the final byte in the output.
 *
 * Note that the transformation performed by this function is to be done
 * in an "online" fashion: output is produced incrementally as input is read,
 * without having to read the entire input first.
 * Note also that this function does *not* attempt to "close" its input
 * or output streams.
 *
 * @return 0 if the operation completes without error, -1 otherwise.
 */
int mtf_encode() {

    char outputbuffer = 0;
    int counter = 0;
    current_offset = 0;
    first_unused_node_index = 0;
    recycled_node_list = NULL;
    mtf_map = node_pool;
    first_unused_node_index++;
    int inputvalue1, inputvalue2, finalvalue, codevalue;
    if (global_options & 1)
    {
        inputvalue2 = getchar();
        inputvalue1 = 0;
    }
    else
    {
        inputvalue1 = getchar();
        inputvalue2 = getchar();
        if (inputvalue1 != EOF && inputvalue2 == EOF)
        {
            fprintf(stderr, "Invalid odd number of bits\n");
            return -1;
        }
    }

    while (inputvalue2 != EOF)
    {
        finalvalue = (inputvalue1*256) + inputvalue2;
        //debug("symbol value: %d\n", finalvalue);
        codevalue = mtf_map_encode(finalvalue);
        if (codevalue == NO_SYMBOL) return -1;
        //debug("code value: %d\n", codevalue);
        FibLessthanGivenNumber(&outputbuffer, &counter, codevalue+1);
        //debug("Final Index position: %d\n", highestFibNum);
        //adding 1 after each symbol
        if (counter+1 > 8)
        {
            //debug("outside fn add 1 put-char: %d", counter);
            //debug("putchar value: %hhx\n", outputbuffer);
            putchar(outputbuffer);
            outputbuffer = 0;
            counter = 1;
            outputbuffer |= 1 << (8 - counter);
        }
        else
        {
            counter = counter + 1;
            outputbuffer |= 1 << (8 - counter);
        }

        if (global_options & 1)
        {
            inputvalue1 = 0;
            inputvalue2 = getchar();
        }
        else
        {
            inputvalue1 = getchar();
            inputvalue2 = getchar();
            if (inputvalue1 != EOF && inputvalue2 == EOF)
            {
                fprintf(stderr, "Invalid odd number of bits\n");
                return -1;
            }
        }
    }

    //outputing the last bites, others bit of char will be zero.
    if (counter > 0)
    {
        //debug("last put-char: %d", counter);
        //debug("putchar value: %hhx\n", outputbuffer);
        putchar(outputbuffer);
    }

    return 0;
}

/**
 * @brief  Perform data decompression, inverting the transformation performed
 * by mtf_encode().
 * @details Read compressed data from stdin and write the corresponding
 * uncompressed data to stdout, inverting the transformation performed by
 * mtf_encode().
 *
 * Data is read byte-by-byte from stdin and is parsed into individual
 * Fibonacci code words, using the fact that two consecutive '1' bits can
 * occur only at the end of a code word.  The terminating '1' bits are
 * discarded and the remaining bits are interpreted as describing the set
 * of terms in the Zeckendorf sum representing a positive integer.
 * The value of the sum is computed, and one is subtracted from it to
 * recover a rank.  Ranks in the upper half of the range of possible values
 * are interpreted as the default ranks of symbols that have not yet been
 * seen, and ranks in the lower half of the range are interpreted as ranks
 * of symbols that have previously been seen.  Using this interpretation,
 * together with the ranking information maintained by the "move-to-front"
 * heuristic, the rank is decoded to obtain a symbol value.  Each symbol
 * value is output as a sequence of one or two bytes (using "big-endian" byte
 * order), according to the value of the BYTES field in the global_options
 * variable.
 *
 * Any 0 bits that occur as padding after the last code word are discarded
 * and do not contribute to the output.
 *
 * Note that (as for mtf_encode()) the transformation performed by this
 * function is to be done in an "online" fashion: the entire input need not
 * (and should not) be read before output is produced.
 * Note also that this function does *not* attempt to "close" its input
 * or output streams.
 *
 * @return 0 if the operation completes without error, -1 otherwise.
 */
int mtf_decode()
{
    if (global_options & 1)
    {
        int inputvalue, Outvalue = 0;
        int counter = 1;
        inputvalue = getchar();
        int RecentOneFlag = 0;
        current_offset = 0;
        first_unused_node_index = 0;
        recycled_node_list = NULL;
        mtf_map = node_pool;
        first_unused_node_index++;

        while (inputvalue != EOF)
        {
            //debug("input value: %d\n", inputvalue);
            for (int i = 1; i < 9; i++)
            {
                if (inputvalue & ((1 << (8 - i))))
                {
                    if (RecentOneFlag)
                    {
                        Outvalue = mtf_map_decode(Outvalue - 1);
                        if (Outvalue == NO_SYMBOL) return -1;
                        putchar(Outvalue);
                        //debug("ouput value: %d\n", Outvalue);
                        counter = 0;
                        RecentOneFlag = 0;
                        Outvalue = 0;
                    }
                    else
                    {
                        Outvalue = Outvalue + FibNum(counter);
                        RecentOneFlag = 1;
                    }

                }
                else
                {
                    RecentOneFlag = 0;
                }
                counter++;
            }
            //debug("ouput after each iteration: %d\n", Outvalue);
            inputvalue = getchar();
        }
        return 0;
    }
    else
    {
        int inputvalue, Outvalue = 0;
        int counter = 1;
        inputvalue = getchar();
        int RecentOneFlag = 0;
        current_offset = 0;
        first_unused_node_index = 0;
        recycled_node_list = NULL;
        mtf_map = node_pool;
        first_unused_node_index++;

        while (inputvalue != EOF)
        {
            //debug("input value: %d\n", inputvalue);
            for (int i = 1; i < 9; i++)
            {
                if (inputvalue & ((1 << (8 - i))))
                {
                    if (RecentOneFlag)
                    {
                        Outvalue = mtf_map_decode(Outvalue - 1);
                        int lstbitsvalue = Outvalue | 256;
                        Outvalue = Outvalue >> 8;
                        //debug("ouput value: %d\n", Outvalue);
                        //debug("lst bits: %d\n", lstbitsvalue);
                        putchar(Outvalue);
                        putchar(lstbitsvalue);
                        counter = 0;
                        RecentOneFlag = 0;
                        Outvalue = 0;
                    }
                    else
                    {
                        Outvalue = Outvalue + FibNum(counter);
                        RecentOneFlag = 1;
                    }

                }
                else
                {
                    RecentOneFlag = 0;
                }
                counter++;
            }
            //debug("ouput after each iteration: %d\n", Outvalue);
            inputvalue = getchar();
        }

        return 0;
    }
    return -1;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the various options that were specified will be
 * encoded in the global variable 'global_options', where it will be
 * accessible elsewhere in the program.  For details of the required
 * encoding, see the assignment handout.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain an encoded representation
 * of the selected program options.
 */

int validargs(int argc, char **argv) {

    if (argc < 2) return -1;

    if (stringcompare(argv[1], "-h"))
    {
        //debug("-h validation success");
        global_options = 0x80000000;
        return 0;
    }

    //if the first argument is not h, then it has to be -e/-d
    int encode = 0, decode = 0;
    if (stringcompare(argv[1], "-e")) encode = 1;
    else if (stringcompare(argv[1], "-d")) decode = 1;

    if ((argc == 2) && (encode || decode))
    {
        if (encode)
            global_options = 0x40000001;
        else if (decode)
            global_options = 0x20000001;

        return 0;
    }

    if ((argc == 4) && (encode || decode))
    {
        //have to check -b flag
        int bflag = 0, bytesisone = 0, bytesistwo = 0;
        if (stringcompare(argv[2], "-b")) bflag = 1;
        if (bflag == 0) return -1;
        int ret = stringcompareforbytes(argv[3]);
        if (ret == 1) bytesisone = 1;
        if (ret == 2) bytesistwo = 1;
        if (bytesisone == 0 && bytesistwo == 0) return -1;
        if (encode && bytesisone) global_options = 0x40000001;
        else if (encode && bytesistwo) global_options = 0x40000002;
        else if (decode && bytesisone) global_options = 0x20000001;
        else if (decode && bytesistwo) global_options = 0x20000002;
        return 0;
    }

    return -1;
}