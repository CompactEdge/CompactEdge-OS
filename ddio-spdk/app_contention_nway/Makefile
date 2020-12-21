include ../includes/Makefile.includes

NPF_FLAGS := --force-retest --show-full


run:
	${NPF_PATH}/npf-run.py local --testie ./app_contention_nway.testie --cluster ${NPF_CLUSTER} --config graph_type=boxplot graph_y_group={result:all} --output --output-columns x all --max-results --graph-filename app_cont-results.pdf --graph-size 10 5 --variable ${TOOLS_PATH} $(SPDK_DEVNAME)


force-run:
	${NPF_PATH}/npf-run.py local --testie ./app_contention_nway.testie --cluster ${NPF_CLUSTER} --config graph_type=boxplot graph_y_group={result:all} --output --output-columns x all --max-results --graph-filename app_cont-results.pdf --graph-size 10 5 --variable ${TOOLS_PATH} ${NPF_FLAGS} $(SPDK_DEVNAME)


clean:
	rm -fr splash-results* testie*/ 
	rm -fr results/