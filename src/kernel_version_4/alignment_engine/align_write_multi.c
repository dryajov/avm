/*
 * THIS  SOFTWARE   CONTAINS  CONFIDENTIAL  INFORMATION
 * AND TRADE SECRETS OF N.L. VERMA / DATUMSOFT SYSTEMS.
 * USE, DISCLOSURE, COPY OR  REPRODUCTION IS PROHIBITED
 * WITHOUT  THE  PRIOR  EXPRESS  WRITTEN  PERMISSION OF
 * NL VERMA / DATUMSOFT SYSTEMS.
 */

#include "prototypes.h"

extern U64 				fec_table_size;
extern U64				fec_active_count;

extern PDZ_THREAD_POOL 	dedupe_thread_pool;

extern SPINLOCK			fec_active_lock;
extern SPINLOCK			fec_flush_lock;

//For operational purpose
extern PFEC_TABLE 		fec_tables_active;
extern PFEC_TABLE 		fec_tables_flush;
extern FEC_WRITE		fec_table_active;
extern PIOREQUEST		fec_flush_iorequest;

extern PDZ_THREAD_POOL align_read_thread_pool;
extern PLBA_BLOCK		lba_table;
extern ATOMIC64 fec_iocount_writes_partial_page;
extern ATOMIC64 fec_iocount_writes_single_page;
extern ATOMIC64 fec_iocount_writes_single_aligned_page;
extern ATOMIC64 fec_iocount_writes_single_unaligned_page;
extern ATOMIC64 fec_iocount_writes_multi_page;

RVOID dz_faw_multi_block_read_page_async_parent_biodone(PBIO bio)
{
	PIOREQUEST ciorequest = (PIOREQUEST) bio->bi_private;
	PIOREQUEST piorequest = NULL;
	if (ciorequest->ior_parent) {
		piorequest = ciorequest->ior_parent;
		PRINT_ATOMIC(piorequest->ior_child_cnt);
		if (atomic_dec_and_test(&piorequest->ior_child_cnt)) {
			// Now complete the Parent / Original bio
			//print_biom(piorequest->ior_bio, "Parent bio when all childs are done");
			LOGFEC("Parent IO Done\n");
			//IO_DONE_STATUS(piorequest, error);
			DZ_BIO_SET_STATUS(piorequest->ior_bio, DZ_BIO_GET_STATUS(bio));
			IOREQUEST_DONE(piorequest);
			DZ_OS_KERNEL_BIO_PUT(bio);
			iorequest_put(ciorequest);

			bio_data_dir(piorequest->ior_bio) == WRITE ? 
			iorequest_put(piorequest) : 
			iorequest_put(piorequest);
			return;
		}
	}
	DZ_OS_KERNEL_BIO_PUT(bio);
	iorequest_put(ciorequest);
	
	return;
}

RVOID dz_faw_multi_page_read_page_async_biodone_head(PBIO bio)
{
	PIOREQUEST	ciorequest 	= (PIOREQUEST)(bio->bi_private);
	PFEC_WRITE	fecw 		= (PFEC_WRITE)(ciorequest->ior_private);
	PVOID   	pagebuf		= NULL;
	BIOVEC		pbvec;
	//INT 		ret 		= SUCCESS;
	INT			bv_offset	= ciorequest->ior_bv_offset;
	ITERATOR iter;
	//if (unlikely(!bio_flagged(bio, BIO_UPTODATE) && !error)) { error = -EIO; }

	
	LOGFEC("Inside biodone Head\n");
	PRINT_POINTER(fecw);
	//ret = test_bit(BIO_UPTODATE, &bio->bi_flags);
	//print_fecws(fecw, "Head First");

	bio_for_each_segment(pbvec, bio, iter) {

		pagebuf = kmap(pbvec.bv_page);
		//Here bv_offset is already set.
		//So it will act as a length i.e
		//from 0 to bv_offset will be the length of the 
		//old data present in the bio
		//Also fecw is already filled with application
		//IO data which is partial in nature
		PMEMCPY(fecw->bv_page, pagebuf, bv_offset );
		kunmap(pbvec.bv_page);
	}

	SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_COMPLETED);

	//TODO:dz_faw_multi_block_read_page_async_parent_biodone(bio, error);
	dz_faw_multi_block_read_page_async_parent_biodone(bio);

	FEC_ADD_WRITE_BUFFER_TO_LBA_TABLE(fecw, ciorequest->ior_lba);

}

RVOID dz_faw_multi_page_read_page_async_biodone_tail(PBIO bio)
{
	PIOREQUEST	ciorequest 	= (PIOREQUEST)(bio->bi_private);
	PFEC_WRITE	fecw 	= (PFEC_WRITE)(ciorequest->ior_private);
	PVOID   	pagebuf	= NULL;
	BIOVEC		pbvec;
	//INT 		ret 	= SUCCESS;
	INT			bv_len	= ciorequest->ior_bv_len;
	ITERATOR itr;

	//if (unlikely(!bio_flagged(bio, BIO_UPTODATE) && !error)) { error = -EIO; }

	LOGFEC("Inside biodone Tail\n");
	PRINT_POINTER(fecw);
	//ret = test_bit(BIO_UPTODATE, &bio->bi_flags);
	//print_fecws(fecw, "Tail First");

	bio_for_each_segment(pbvec, bio, itr) {
		pagebuf = kmap(pbvec.bv_page);
		pagebuf += bv_len;
		PMEMCPY((fecw->bv_page  + bv_len), pagebuf, PAGE_SIZE - bv_len);

		kunmap(pbvec.bv_page);
	}

	SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_COMPLETED);

	//TODO:dz_faw_multi_block_read_page_async_parent_biodone(bio, error);
	dz_faw_multi_block_read_page_async_parent_biodone(bio);

	FEC_ADD_WRITE_BUFFER_TO_LBA_TABLE(fecw, ciorequest->ior_lba);
	//iorequest_put(iorequest);

}

PIOREQUEST dz_faw_multi_block_read_page_async(PIOREQUEST piorequest, PBIO parent_bio,
		SECTOR sector, PBIOVEC biovec, BIODONE biodone)
{
	PPAGE      	page		= NULL;
	PBIO 		bio 		= NULL;
	PVOID  		pagebuf		= NULL;
	PIOREQUEST	iorequest 	= NULL;

	iorequest = dz_io_alloc();
	if (!iorequest) {
		LOGFECE("Unable to allocate memory for iorequest\n");
		RETURNN;
	}
	memset(iorequest, 0, sizeof(IOREQUEST));

	page =  dz_read_page_alloc();
	if (!page)  {
		LOGFECE("Unable to get free read page\n");
		dz_io_free(iorequest);
		RETURNN;
	}

	bio = dz_bio_alloc(1);
	if (!bio) {
		LOGFECE("Unable to get free bio\n");
		dz_io_free(iorequest);
		dz_read_page_free(page);
		RETURNN;
	}

	pagebuf = kmap(page);
	memset(pagebuf, 0, PAGE_SIZE);
	kunmap(page);

	biovec->bv_page = page;
	biovec->bv_offset = 0;
	biovec->bv_len = PAGE_SIZE;

	//bio->bi_bdev 	= parent_bio->bi_bdev;
	DZ_BIO_SET_BDEV(bio, DZ_BIO_GET_BDEV(parent_bio));
	DZ_BIO_SET_SECTOR(bio, sector);
	DZ_BIO_SET_BIODONE(bio, biodone);
	DZ_BIO_SET_ZIDX(bio);
	//bio->bi_rw 		= READ;
	DZ_BIO_SET_READ(bio);
	DZ_BIO_SET_NEXTN(bio);
	bio->bi_private = iorequest;
	//bio->bi_flags   = 1 << BIO_UPTODATE;
	//bio->bi_size	= bio_size;

	if (!bio_add_page(bio, page, biovec->bv_len, biovec->bv_offset)) {
			LOGFECE("Unable to add page to bio\n");
			dz_io_free(iorequest);
			dz_read_page_free(page);
			DZ_OS_KERNEL_BIO_PUT(bio); // It will free the bio as well
			RETURNN;
	}
	iorequest->ior_bio 			= bio;
	iorequest->ior_thread_pool 	= align_read_thread_pool;
	iorequest->ior_parent 		= piorequest;
	atomic_inc(&piorequest->ior_child_cnt);

	return iorequest;
}

// This function will not allocate any new pages
// It essentially means that no splitting of bios will occur here.
// which will help us to do the application io done here itself.
RVOID dz_faw_multi_block_all_aligned(PIOREQUEST piorequest)
{
	PFEC_WRITE	fecw				= NULL;
	PBIO		bio 				= piorequest->ior_bio; // Parent or Original bio
	//TODO:SIZE  		io_size				= bio->bi_size; // Parent or Original bio size
	SIZE  		io_size				= DZ_BIO_GET_SIZE(bio); // Parent or Original bio size
	SECTOR		sector 				= DZ_BIO_GET_SECTOR(bio);
	LBA			lba					= piorequest->ior_lba;
	PVOID		pfecwpage 			= NULL;
	PBIOVEC		cur_bvec  			= NULL;
	INT			bi_vcnt				= 0;
	INT			bi_vcnt_max			= 0;
	INT			bv_len				= 0;
	INT			remaining_bv_len	= 0;
	UINT		sector_pos_in_lba	= 0;
	PVOID   	pagebuf				= NULL;
	SIZE		sector_bytes		= 0;
	U64			tot_bytes			= 0;
	INT			data_bytes			= 0;
	INT 		aligned_bytes 		= 0;
	INT			total_blocks_needed	= 0;
	INT			count				= 0;
	LIST_HEAD	free_list_head;


	//TODO: Optimization required. Try to use bit shifting to simplify this
	sector_bytes 		= sector * SECTOR_SIZE;
	sector_pos_in_lba 	= sector_bytes   - (lba * LBA_BLOCK_SIZE);
	tot_bytes 			= sector_bytes + io_size;
	aligned_bytes 		= io_size ; //Always in multiples of LBA_BLOCK_SIZE

	//Calculate total blocks: one for head, one for tail and ...
	total_blocks_needed = (aligned_bytes / LBA_BLOCK_SIZE);
	INIT_LIST_HEAD(&free_list_head);

	//Allocate that many write blocks

	for (count = 0; count < total_blocks_needed; count++) {
		//PRINT_POINTER(fecw);
		list_add_tail(&(fecw->ioq), &free_list_head);
		SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_IN_PROGRESS);
		fecw++;
	}

	//LOGFEC("Printing Aligned part\n");

	//Now copy the intermediate aligned data. It can be of any no. of pages
	//Also the data in fecw page will always be stored in zeroth offset.
	//This operation will not include any RMWs because the data is block/Page
	//aligned. So there is no need to issue a Read bio
	//Note continuing with previous bi_vcnt and cur_bvec
	bi_vcnt_max = bio->bi_vcnt;
	
	while(aligned_bytes) {

		fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
		list_del(&fecw->ioq);
		//PRINT_POINTER(fecw);

		fecw->lba		= lba;
		data_bytes 		= PAGE_SIZE;
		pfecwpage 		= fecw->bv_page;

		if (remaining_bv_len) {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset + (cur_bvec->bv_len - remaining_bv_len);
				PMEMCPY(pfecwpage, pagebuf, remaining_bv_len);
				kunmap(cur_bvec->bv_page);
				pfecwpage 		+= remaining_bv_len;
				data_bytes 		-= remaining_bv_len;
				bi_vcnt++;
				remaining_bv_len = 0;
		}

		while(bi_vcnt < bi_vcnt_max) {
			cur_bvec 	= &bio->bi_io_vec[bi_vcnt];
			bv_len 		= cur_bvec->bv_len;

			if (bv_len >= data_bytes) {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset;

				PMEMCPY(pfecwpage, pagebuf, data_bytes);
				kunmap(cur_bvec->bv_page);

				if (bv_len == data_bytes) {
					bi_vcnt++;
				} else {
					remaining_bv_len = bv_len - data_bytes;
				}
				aligned_bytes -= PAGE_SIZE;
				//print_fecws(fecw, "Aligned Part");
				SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_COMPLETED);
				FEC_ADD_WRITE_BUFFER_TO_LBA_TABLE(fecw, lba);

				break;
			} else {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset;

				PMEMCPY(pfecwpage, pagebuf, bv_len);
				kunmap(cur_bvec->bv_page);

				data_bytes -= bv_len;
				bi_vcnt++;
				pfecwpage += bv_len;

			}
		} // End of Inner While loop
		lba++;
	} //End of Outer While loop

	WRITE_IO_DONE(piorequest);
	iorequest_put(piorequest);
	//There should not be any parent to this piorequest
	if (piorequest->ior_parent) {
		BUG_ON(1);
	}
	return;

}

//Here the start offset will be page aligned
//but the end (referred as tail) will be less than
//PAGE_SIZE
//Top level application IO will be done inside the callback of
//tail bio
RVOID dz_faw_multi_block_tail_unaligned(PIOREQUEST piorequest)
{
	PFEC_WRITE	fecw				= NULL;
	PBIO		bio 				= piorequest->ior_bio; // Parent or Original bio
	//TODO:SIZE  		io_size				= bio->bi_size; // Parent or Original bio size
	SIZE  		io_size				= DZ_BIO_GET_SIZE(bio); // Parent or Original bio size
	SECTOR		sector 				= DZ_BIO_GET_SECTOR(bio);
	LBA			lba					= piorequest->ior_lba;
	PVOID		pfecwpage 			= NULL;
	PBIOVEC		cur_bvec  			= NULL;
	INT			bi_vcnt				= 0;
	INT			bi_vcnt_max			= 0;
	INT			bv_len				= 0;
	INT			remaining_bv_len	= 0;
	UINT		sector_pos_in_lba	= 0;
	PVOID   	pagebuf				= NULL;
	SIZE		sector_bytes		= 0;
	U64			tot_bytes			= 0;
	INT			data_bytes			= 0;
	INT 		aligned_bytes 		= 0;
	INT			partial_bytes_tail 	= 0;
	PIOREQUEST	iorequest_tail		= NULL;
	BIOVEC		biovec2;
	INT			total_blocks_needed	= 0;
	INT			count				= 0;
	LIST_HEAD	free_list_head		;


	//TODO: Optimization required. Try to use bit shifting to simplify this
	sector_bytes 		= sector * SECTOR_SIZE;
	sector_pos_in_lba 	= sector_bytes   - (lba * LBA_BLOCK_SIZE);
	tot_bytes 			= sector_bytes + io_size;
	partial_bytes_tail 	= tot_bytes % LBA_BLOCK_SIZE;
	aligned_bytes 		= io_size - partial_bytes_tail ; //Always in multiples of LBA_BLOCK_SIZE

	//Calculate total blocks: one for tail and ...
	total_blocks_needed = 1 + (aligned_bytes / LBA_BLOCK_SIZE);
	
	INIT_LIST_HEAD(&free_list_head);
	//Allocate that many write blocks
	/*
	if (!(fecw = GET_FEC_MULTIPLE_FREE_BUFFERS(total_blocks_needed))) {
		goto exit_failure;
	}
	*/
	for (count = 0; count < total_blocks_needed; count++) {
		PRINT_POINTER(fecw);
		list_add_tail(&(fecw->ioq), &free_list_head);
		SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_IN_PROGRESS);
		fecw++;
	}
	
	atomic_set(&piorequest->ior_child_cnt, 0);

	//Create a bio for Tail Part. Because it involves RMW operation
	//MW will be taken carre inside the done callback function
	//Application IO will also be done inside the done callback
	//function
	iorequest_tail = dz_faw_multi_block_read_page_async(piorequest, bio, sector, &biovec2,
			dz_faw_multi_page_read_page_async_biodone_tail);
	if (!iorequest_tail) {
		goto exit_failure;
	}

	//By now we are done with resource allocation part.
	//Now we need to copy the parent bio pages in their
	//corresponding fecws
 
	print_bio(bio);

	LOGFEC("Printing Head Aligned part\n");

	//Now copy the head aligned data. It can be of any no. of pages
	//Also the data in fecw page will always be stored in zeroth offset.
	//This operation will not include any RMWs because the data is block/Page
	//aligned. So there is no need to issue a Read bio
	
	bi_vcnt_max = bio->bi_vcnt;
	while(aligned_bytes) {

		fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
		list_del(&fecw->ioq);
		PRINT_POINTER(fecw);

		fecw->lba		= lba;
//		fecw->bv_offset	= 0;
//		fecw->bv_len	= PAGE_SIZE;
		data_bytes 		= PAGE_SIZE;
		pfecwpage 		= fecw->bv_page;

		if (remaining_bv_len) {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset + (cur_bvec->bv_len - remaining_bv_len);
				PMEMCPY(pfecwpage, pagebuf, remaining_bv_len);
				kunmap(cur_bvec->bv_page);
				pfecwpage 		+= remaining_bv_len;
				data_bytes 		-= remaining_bv_len;
				bi_vcnt++;
				remaining_bv_len = 0;

		}

		while(bi_vcnt < bi_vcnt_max) {
			cur_bvec 	= &bio->bi_io_vec[bi_vcnt];
			bv_len 		= cur_bvec->bv_len;

			if (bv_len >= data_bytes) {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset;

				PMEMCPY(pfecwpage, pagebuf, data_bytes);
				kunmap(cur_bvec->bv_page);

				if (bv_len == data_bytes) {
					bi_vcnt++;
				} else {
					remaining_bv_len = bv_len - data_bytes;
				}
				aligned_bytes -= PAGE_SIZE;
				SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_COMPLETED);
				FEC_ADD_WRITE_BUFFER_TO_LBA_TABLE(fecw, lba);
				break;
			} else {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset;

				PMEMCPY(pfecwpage, pagebuf, bv_len);
				kunmap(cur_bvec->bv_page);

				data_bytes -= bv_len;
				bi_vcnt++;
				pfecwpage += bv_len;

			}
		} // End of Inner While loop
		sector += SECTORS_PER_PAGE;
		lba++;
	} //End of Outer While loop

	//Now copy the unaligned tail part. Aligned tail part is already
	//included in the above loop

	LOGFEC("Printing Tail part\n");
	//Extract last entry of fecw from free list

	if (!(list_empty(&free_list_head))) {
		BUG_ON(1);
	}
	fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
	list_del(&fecw->ioq);
	PRINT_POINTER(fecw);

	iorequest_tail->ior_lba 		= lba;
	iorequest_tail->ior_private 	= fecw;
	iorequest_tail->ior_bv_offset 	= 0;
	iorequest_tail->ior_bv_len 		= partial_bytes_tail;
	fecw->lba					= lba;
	//fecw->bv_offset			= 0;
	//fecw->bv_len			= partial_bytes_tail;
	data_bytes 					= partial_bytes_tail;
	pfecwpage 					= fecw->bv_page;

	//Note continuing with previous bi_vcnt
	if (remaining_bv_len) {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset + (cur_bvec->bv_len - remaining_bv_len);
			PMEMCPY(pfecwpage, pagebuf, remaining_bv_len);
			kunmap(cur_bvec->bv_page);
			pfecwpage 		+= remaining_bv_len;
			data_bytes 		-= remaining_bv_len;
			bi_vcnt++;
			remaining_bv_len = 0;

	}
	
	while(bi_vcnt < bi_vcnt_max) {
		cur_bvec 	= &bio->bi_io_vec[bi_vcnt];
		bv_len 		= cur_bvec->bv_len;

		if (bv_len >= data_bytes) {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset;

			PMEMCPY(pfecwpage, pagebuf, data_bytes);
			kunmap(cur_bvec->bv_page);

			if (bv_len == data_bytes) {
				bi_vcnt++;
			} else {
				remaining_bv_len = bv_len - data_bytes;
			}
			//print_fecw(fecw);
			break;
		} else {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset;

			PMEMCPY(pfecwpage, pagebuf, bv_len);
			kunmap(cur_bvec->bv_page);

			data_bytes -= bv_len;
			bi_vcnt++;
			pfecwpage += bv_len;

		}
	} // End of Inner While loop

	//Enqueue the tail child bio
	dz_q_iorequest_thread_pool(iorequest_tail);
	return;

exit_failure:
	DELAY_MICRO_SECONDS(1);
	IO_DONE_BUSY(piorequest);
	for (count = 0; count < total_blocks_needed; count++) {

		fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
		if (!fecw) continue;
		list_del(&fecw->ioq);
		FEC_ENTRY_WRITE_LOCK(fecw);
		fecw->state = FECBUF_STATE_IDLE;
		FEC_ENTRY_WRITE_UNLOCK(fecw);
	}
}

//Here the start offset will be page unaligned (also
//referred as head) but the end will be aligned to PAGE_SIZE
//Top level application IO will be done inside the callback of
//head bio

RVOID dz_faw_multi_block_head_unaligned(PIOREQUEST piorequest)
{
	PFEC_WRITE	fecw				= NULL;
	PBIO		bio 				= piorequest->ior_bio; // Parent or Original bio
	//TODO:SIZE  		io_size				= bio->bi_size; // Parent or Original bio size
	SIZE  		io_size				= DZ_BIO_GET_SIZE(bio); // Parent or Original bio size
	SECTOR		sector 				= DZ_BIO_GET_SECTOR(bio);
	LBA			lba					= piorequest->ior_lba;
	PVOID		pfecwpage 			= NULL;
	PBIOVEC		cur_bvec  			= NULL;
	INT			bi_vcnt				= 0;
	INT			bi_vcnt_max			= 0;
	INT			bv_len				= 0;
	INT			remaining_bv_len	= 0;
	UINT		sector_pos_in_lba	= 0;
	PVOID   	pagebuf				= NULL;
	SIZE		sector_bytes		= 0;
	U64			tot_bytes			= 0;
	INT			data_bytes			= 0;
	INT 		aligned_bytes 		= 0;
	INT			partial_bytes_head 	= 0;
	PIOREQUEST	iorequest_head		= NULL;
	BIOVEC		biovec;
	INT			total_blocks_needed	= 0;
	INT			count				= 0;
	LIST_HEAD	free_list_head		;


	//TODO: Optimization required. Try to use bit shifting to simplify this
	sector_bytes 		= sector * SECTOR_SIZE;
	sector_pos_in_lba 	= sector_bytes   - (lba * LBA_BLOCK_SIZE);
	tot_bytes 			= sector_bytes + io_size;
	partial_bytes_head 	= LBA_BLOCK_SIZE - sector_pos_in_lba;
	aligned_bytes 		= io_size - partial_bytes_head ; //Always in multiples of LBA_BLOCK_SIZE

	//Calculate total blocks: one for head, and ...
	total_blocks_needed = 1 + (aligned_bytes / LBA_BLOCK_SIZE);

	INIT_LIST_HEAD(&free_list_head);
	//Allocate that many write blocks
	/*
	if (!(fecw = GET_FEC_MULTIPLE_FREE_BUFFERS(total_blocks_needed))) {
		goto exit_failure;
	}
	*/
	for (count = 0; count < total_blocks_needed; count++) {
		PRINT_POINTER(fecw);
		list_add_tail(&(fecw->ioq), &free_list_head);
		SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_IN_PROGRESS);
		fecw++;
	}

	atomic_set(&piorequest->ior_child_cnt, 0);

	//Create a bio for Head Part. Because it involves RMW operation
	//MW will be taken carre inside the done callback function
	iorequest_head = dz_faw_multi_block_read_page_async(piorequest, bio, sector, &biovec,
			dz_faw_multi_page_read_page_async_biodone_head);
	if (!iorequest_head) {
		goto exit_failure;
	} 

	//By now we are done with resource allocation part.
	//Now we need to copy the parent bio pages in their
	//corresponding fecws
 
	print_bio(bio);

	//Extract the first entry of fecw from free list
	fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
	list_del(&fecw->ioq);
	PRINT_POINTER(fecw);

	fecw->lba 				= lba;
	iorequest_head->ior_private = fecw;
	iorequest_head->ior_lba		= lba;
	iorequest_head->ior_bv_offset	= sector_pos_in_lba;

	//fecw->bv_offset		= sector_pos_in_lba;
	//fecw->bv_len		= partial_bytes_head;

	pfecwpage = fecw->bv_page + sector_pos_in_lba;
	bi_vcnt_max = bio->bi_vcnt;
	data_bytes = partial_bytes_head;
	LOGFEC("Printing Head part\n");

	// Copy the head part of unaligned data. This will be less than a block size
	// and hence will include a Read Modify Write operation
	// Begin from zeroth index of bio vec array
	while(bi_vcnt < bi_vcnt_max) {
		cur_bvec = &bio->bi_io_vec[bi_vcnt];
		bv_len = cur_bvec->bv_len;

		if (bv_len >= data_bytes) {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset;

			PMEMCPY(pfecwpage, pagebuf, data_bytes);
			kunmap(cur_bvec->bv_page);

			if (bv_len == data_bytes) {
				bi_vcnt++;
			} else {
				remaining_bv_len = bv_len - data_bytes;

			}
			break;
		} else {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset;

			PMEMCPY(pfecwpage, pagebuf, bv_len);
			kunmap(cur_bvec->bv_page);

			data_bytes -= bv_len;
			bi_vcnt++;
			pfecwpage += bv_len;

		}
	} // End of While loop

	sector += (partial_bytes_head  / SECTOR_SIZE);
	lba++;


	LOGFEC("Printing Tail Aligned part\n");

	//Now copy the intermediate aligned data. It can be of any no. of pages
	//Also the data in fecw page will always be stored in zeroth offset.
	//This operation will not include any RMWs because the data is block/Page
	//aligned. So there is no need to issue a Read bio
	//Note continuing with previous bi_vcnt and cur_bvec
	
	while(aligned_bytes) {

		fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
		list_del(&fecw->ioq);
		PRINT_POINTER(fecw);

		fecw->lba		= lba;
		//fecw->bv_offset	= 0;
		//fecw->bv_len	= PAGE_SIZE;
		data_bytes 		= PAGE_SIZE;
		pfecwpage 		= fecw->bv_page;

		if (remaining_bv_len) {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset + (cur_bvec->bv_len - remaining_bv_len);
				PMEMCPY(pfecwpage, pagebuf, remaining_bv_len);
				kunmap(cur_bvec->bv_page);
				pfecwpage 		+= remaining_bv_len;
				data_bytes 		-= remaining_bv_len;
				bi_vcnt++;
				remaining_bv_len = 0;

		}

		while(bi_vcnt < bi_vcnt_max) {
			cur_bvec 	= &bio->bi_io_vec[bi_vcnt];
			bv_len 		= cur_bvec->bv_len;

			if (bv_len >= data_bytes) {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset;

				PMEMCPY(pfecwpage, pagebuf, data_bytes);
				kunmap(cur_bvec->bv_page);

				if (bv_len == data_bytes) {
					bi_vcnt++;
				} else {
					remaining_bv_len = bv_len - data_bytes;
				}
				aligned_bytes -= PAGE_SIZE;
				SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_COMPLETED);
				FEC_ADD_WRITE_BUFFER_TO_LBA_TABLE(fecw, lba);
				break;
			} else {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset;

				PMEMCPY(pfecwpage, pagebuf, bv_len);
				kunmap(cur_bvec->bv_page);

				data_bytes -= bv_len;
				bi_vcnt++;
				pfecwpage += bv_len;

			}
		} // End of Inner While loop
		sector += SECTORS_PER_PAGE;
		lba++;
	} //End of Outer While loop

	//Enqueue the head child bio
	dz_q_iorequest_thread_pool(iorequest_head);
	return;

exit_failure:
	DELAY_MICRO_SECONDS(1);
	IO_DONE_BUSY(piorequest);
	for (count = 0; count < total_blocks_needed; count++) {

		fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
		if (!fecw) continue;
		list_del(&fecw->ioq);
		FEC_ENTRY_WRITE_LOCK(fecw);
		fecw->state = FECBUF_STATE_IDLE;
		FEC_ENTRY_WRITE_UNLOCK(fecw);
	}
}

//Here the start offset will be page unaligned (also
//referred as head) and end offset (tail) will also be page
//unaligned, but the intermediate ones will be page aligned.
//Here two child bios will be created one for head and one for
//tail.
//Top level application IO will be done when both head and tail
//are done
RVOID dz_faw_multi_block_both_unaligned(PIOREQUEST piorequest)
{
	PFEC_WRITE	fecw				= NULL;
	PBIO		bio 				= piorequest->ior_bio; // Parent or Original bio
	//TODO:SIZE  		io_size				= bio->bi_size; // Parent or Original bio size
	SIZE  		io_size				= DZ_BIO_GET_SIZE(bio); // Parent or Original bio size
	SECTOR		sector 				= DZ_BIO_GET_SECTOR(bio);
	LBA			lba					= piorequest->ior_lba;
	PVOID		pfecwpage 			= NULL;
	PBIOVEC		cur_bvec  			= NULL;
	INT			bi_vcnt				= 0;
	INT			bi_vcnt_max			= 0;
	INT			bv_len				= 0;
	INT			remaining_bv_len	= 0;
	UINT		sector_pos_in_lba	= 0;
	PVOID   	pagebuf				= NULL;
	SIZE		sector_bytes		= 0;
	U64			tot_bytes			= 0;
	INT			data_bytes			= 0;
	INT 		aligned_bytes 		= 0;
	INT			partial_bytes_head 	= 0;
	INT			partial_bytes_tail 	= 0;
	PIOREQUEST	iorequest_head		= NULL;
	PIOREQUEST	iorequest_tail		= NULL;
	BIOVEC		biovec;
	BIOVEC		biovec2;
	INT			total_blocks_needed	= 0;
	INT			count				= 0;
	LIST_HEAD	free_list_head		;


	//TODO: Optimization required. Try to use bit shifting to simplify this
	sector_bytes 		= sector * SECTOR_SIZE;
	sector_pos_in_lba 	= sector_bytes   - (lba * LBA_BLOCK_SIZE);
	tot_bytes 			= sector_bytes + io_size;
	partial_bytes_head 	= LBA_BLOCK_SIZE - sector_pos_in_lba;
	partial_bytes_tail 	= tot_bytes % LBA_BLOCK_SIZE;
	aligned_bytes 		= io_size - partial_bytes_head - partial_bytes_tail ; //Always in multiples of LBA_BLOCK_SIZE

	//Calculate total blocks: one for head, one for tail and ...
	total_blocks_needed = 1 + 1 + (aligned_bytes / LBA_BLOCK_SIZE);

	INIT_LIST_HEAD(&free_list_head);
	//Allocate that many write blocks
	/*
	if (!(fecw = GET_FEC_MULTIPLE_FREE_BUFFERS(total_blocks_needed))) {
		goto exit_failure;
	}
	*/
	for (count = 0; count < total_blocks_needed; count++) {
		PRINT_POINTER(fecw);
		list_add_tail(&(fecw->ioq), &free_list_head);
		SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_IN_PROGRESS);
		fecw++;
	}


	atomic_set(&piorequest->ior_child_cnt, 0);

	//Create a bio for Head Part. Because it involves RMW operation
	//MW will be taken carre inside the done callback function
	iorequest_head = dz_faw_multi_block_read_page_async(piorequest, bio, sector, &biovec,
			dz_faw_multi_page_read_page_async_biodone_head);
	if (!iorequest_head) {
		goto exit_failure;
	} 

	//We need to create a Read BIO for tail as well because it involves RMW.
	//MW will be taken care inside the done function
	iorequest_tail = dz_faw_multi_block_read_page_async(piorequest, bio, sector, &biovec2,
			dz_faw_multi_page_read_page_async_biodone_tail);
	if (!iorequest_tail) {
		goto exit_failure;
	}

	//By now we are done with resource allocation part.
	//Now we need to copy the parent bio pages in their
	//corresponding fecws
 
	print_bio(bio);

	//Extract the first entry of fecw from free list
	fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
	list_del(&fecw->ioq);
	PRINT_POINTER(fecw);

	fecw->lba 					= lba;
	iorequest_head->ior_private 	= fecw;
	iorequest_head->ior_lba			= lba;
	iorequest_head->ior_bv_offset 	= sector_pos_in_lba;
	iorequest_head->ior_bv_len 		= partial_bytes_head;

	//fecw->bv_offset		= sector_pos_in_lba;
	//fecw->bv_len		= partial_bytes_head;

	pfecwpage = fecw->bv_page + sector_pos_in_lba;
	bi_vcnt_max = bio->bi_vcnt;
	data_bytes = partial_bytes_head;
	LOGFEC("Printing Head part\n");

	// Copy the head part of unaligned data. This will be less than a block size
	// and hence will include a Read Modify Write operation
	// Begin from zeroth index of bio vec array
	while(bi_vcnt < bi_vcnt_max) {
		cur_bvec = &bio->bi_io_vec[bi_vcnt];
		bv_len = cur_bvec->bv_len;

		if (bv_len >= data_bytes) {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset;

			PMEMCPY(pfecwpage, pagebuf, data_bytes);
			kunmap(cur_bvec->bv_page);

			if (bv_len == data_bytes) {
				bi_vcnt++;
			} else {
				remaining_bv_len = bv_len - data_bytes;

			}
			break;
		} else {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset;

			PMEMCPY(pfecwpage, pagebuf, bv_len);
			kunmap(cur_bvec->bv_page);

			data_bytes -= bv_len;
			bi_vcnt++;
			pfecwpage += bv_len;

		}
	} // End of While loop

	sector += (partial_bytes_head  / SECTOR_SIZE);
	lba++;

	//Enqueue the head child bio
	dz_q_iorequest_thread_pool(iorequest_head);

	LOGFEC("Printing Intermediate Aligned part\n");

	//Now copy the intermediate aligned data. It can be of any no. of pages
	//Also the data in fecw page will always be stored in zeroth offset.
	//This operation will not include any RMWs because the data is block/Page
	//aligned. So there is no need to issue a Read bio
	//Note continuing with previous bi_vcnt and cur_bvec
	
	while(aligned_bytes) {

		fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
		list_del(&fecw->ioq);
		PRINT_POINTER(fecw);

		fecw->lba		= lba;
		//fecw->bv_offset	= 0;
		//fecw->bv_len	= PAGE_SIZE;
		data_bytes 		= PAGE_SIZE;
		pfecwpage 		= fecw->bv_page;

		if (remaining_bv_len) {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset + (cur_bvec->bv_len - remaining_bv_len);
				PMEMCPY(pfecwpage, pagebuf, remaining_bv_len);
				kunmap(cur_bvec->bv_page);
				pfecwpage 		+= remaining_bv_len;
				data_bytes 		-= remaining_bv_len;
				bi_vcnt++;
				remaining_bv_len = 0;

		}

		while(bi_vcnt < bi_vcnt_max) {
			cur_bvec 	= &bio->bi_io_vec[bi_vcnt];
			bv_len 		= cur_bvec->bv_len;

			if (bv_len >= data_bytes) {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset;

				PMEMCPY(pfecwpage, pagebuf, data_bytes);
				kunmap(cur_bvec->bv_page);

				if (bv_len == data_bytes) {
					bi_vcnt++;
				} else {
					remaining_bv_len = bv_len - data_bytes;
				}
				aligned_bytes -= PAGE_SIZE;
		
				SET_FEC_BUFFER_STATE(fecw, FECBUF_STATE_MEMORY_WRITE_COMPLETED);
				FEC_ADD_WRITE_BUFFER_TO_LBA_TABLE(fecw, lba);
				break;
			} else {
				pagebuf = kmap(cur_bvec->bv_page);
				pagebuf += cur_bvec->bv_offset;

				PMEMCPY(pfecwpage, pagebuf, bv_len);
				kunmap(cur_bvec->bv_page);

				data_bytes -= bv_len;
				bi_vcnt++;
				pfecwpage += bv_len;

			}
		} // End of Inner While loop
		sector += SECTORS_PER_PAGE;
		lba++;
	} //End of Outer While loop

	//Now copy the unaligned tail part. Aligned tail part is already
	//included in the above loop

	LOGFEC("Printing Tail part\n");
	//Extract last entry of fecw from free list

	fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
	list_del(&fecw->ioq);
	if (!(list_empty(&free_list_head))) {
		BUG_ON(1);
	}
	PRINT_POINTER(fecw);

	iorequest_tail->ior_lba 		= lba;
	iorequest_tail->ior_private 	= fecw;
	iorequest_tail->ior_bv_offset 	= 0;
	iorequest_tail->ior_bv_len 		= partial_bytes_tail;
	fecw->lba					= lba;
	//fecw->bv_offset				= 0;
	//fecw->bv_len				= partial_bytes_tail;
	data_bytes 					= partial_bytes_tail;
	pfecwpage 					= fecw->bv_page;

	//Note continuing with previous bi_vcnt
	if (remaining_bv_len) {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset + (cur_bvec->bv_len - remaining_bv_len);
			PMEMCPY(pfecwpage, pagebuf, remaining_bv_len);
			kunmap(cur_bvec->bv_page);
			pfecwpage 		+= remaining_bv_len;
			data_bytes 		-= remaining_bv_len;
			bi_vcnt++;
			remaining_bv_len = 0;

	}
	
	while(bi_vcnt < bi_vcnt_max) {
		cur_bvec 	= &bio->bi_io_vec[bi_vcnt];
		bv_len 		= cur_bvec->bv_len;

		if (bv_len >= data_bytes) {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset;

			PMEMCPY(pfecwpage, pagebuf, data_bytes);
			kunmap(cur_bvec->bv_page);

			if (bv_len == data_bytes) {
				bi_vcnt++;
			} else {
				remaining_bv_len = bv_len - data_bytes;
			}
			//print_fecw(fecw);
			break;
		} else {
			pagebuf = kmap(cur_bvec->bv_page);
			pagebuf += cur_bvec->bv_offset;

			PMEMCPY(pfecwpage, pagebuf, bv_len);
			kunmap(cur_bvec->bv_page);

			data_bytes -= bv_len;
			bi_vcnt++;
			pfecwpage += bv_len;

		}
	} // End of Inner While loop

	//Enqueue the tail child bio
	dz_q_iorequest_thread_pool(iorequest_tail);
	LOGFEC("Returning after enqueuing tail\n");
	return;

exit_failure:
	DELAY_MICRO_SECONDS(1);
	IO_DONE_BUSY(piorequest);
	for (count = 0; count < total_blocks_needed; count++) {

		fecw = list_first_entry(&free_list_head, union dz_fec_data_write_s, ioq);
		if (!fecw) continue;
		list_del(&fecw->ioq);
		FEC_ENTRY_WRITE_LOCK(fecw);
		fecw->state = FECBUF_STATE_IDLE;
		FEC_ENTRY_WRITE_UNLOCK(fecw);
	}

	if (iorequest_head) {
		dz_read_page_free(bio_page(iorequest_head->ior_bio));
		DZ_OS_KERNEL_BIO_PUT(iorequest_head->ior_bio);
		dz_io_free(iorequest_head);
		return;
	}
}

RVOID dz_align_write_for_multi_block(PIOREQUEST piorequest)
{
	PBIO	bio 			= piorequest->ior_bio; // Parent or Original bio
	SECTOR	sector 			= DZ_BIO_GET_SECTOR(bio);
	//TODO:SIZE  	io_size			= bio->bi_size; // Parent or Original bio size
	SIZE  	io_size			= DZ_BIO_GET_SIZE(bio); // Parent or Original bio size
	SIZE	sector_bytes	= 0;
	U64		tot_bytes		= 0;

	sector_bytes = sector * SECTOR_SIZE;

	// Four cases can exists here:
	// 1. When bio is properly aligned with all pages. It means Head and tail of bio both are Page aligned
	// 2. When head of bio is Page Aligned but not tail.
	// 3. When head is not Page aligned but tail is aligned
	// 4. When head and tail both are not Page aligned
	//
	// In terms of handling: Case 1 and 2 can be handled together.
	// Case 3 and 4 can also be handled together.
	if ((sector_bytes  % LBA_BLOCK_SIZE) == 0 ) { 
	// Start Sector is Page Aligned. Check if tail is also aligned
	  
		if (((sector_bytes + io_size)  % LBA_BLOCK_SIZE) == 0 ) { 
			// Complete bio is page aligned. So child bios will be 1:1 mapping of pages
		
			//LOGFEC("Case1: BIO is multiple blocks aligned i.e. Head and Tail Aligned\n");
			// Case 1
			//LOGFEC("Case1:BIO is Head Aligned and Tail Aligned\n");
			dz_faw_multi_block_all_aligned(piorequest);

		} else {
			// Tail of the bio is NOT page aligned. 
			// Case 2
			// Here last bv_page will partial page data. 
			//LOGFEC("Case2: BIO is Head Aligned but Not Tail Aligned\n");
			LOGFEC("Case2:BIO is Head Aligned and Tail UnAligned\n");
			dz_faw_multi_block_tail_unaligned(piorequest);
		}
	} else if (((tot_bytes = (sector_bytes + io_size))  % LBA_BLOCK_SIZE) == 0 ) { 
		// Head is not Page Aligned, but Tail is Page Aligned
		// Case 3
		// Here First page will be a partial one, rest will be complete ones.
		// Here no. of Pages will be same to child bios

		//LOGFEC("Case3: BIO is Tail Aligned but Not Head Aligned\n");
		
		// Unaligned part of total bytes. It will reside in the Head of bio because tail is always aligned
		LOGFEC("Case3:BIO is Head UnAligned and Tail Aligned\n");
		dz_faw_multi_block_head_unaligned(piorequest);
	} else  {
		// Head and Tail both are not Page Aligned
		// Case 4
		// Here First page will be a partial one.
		// Last Page will also be partial one
		// Here is the processing part
		// Process the unaligned head part
		// Process the unaligned tail part
		// Process the aligned part from tail to head 
		LOGFEC("Case4:BIO is Neither Head Aligned Nor Tail Aligned\n");
		dz_faw_multi_block_both_unaligned(piorequest);
	}
}
