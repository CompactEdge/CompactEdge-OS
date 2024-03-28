
# Features
## Linux Kernel
### PREEMPT-RT Patch
`PREEMPT-RT`는 리눅스 커널의 `realtime` 성능을 끌어올려 실시간 운영체제로써 기능할 수 있도록 만드는 patch set 입니다.

`PREEMPT-RT`는 리눅스 커널을 수정하여 거의 모든 코드 경로에서 선점(preemption)을 가능하게 합니다.

선점이란, 높은 우선순위의 작업이 실행 중인 낮은 우선순위의 작업을 중단시키고 즉시 실행될 수 있도록 하는 기능입니다.

이를 통해 시스템의 응답성과 예측 가능성을 크게 향상시킬 수 있습니다.

`PREEMPT-RT` 패치는 다음과 같은 주요 변경 사항을 포함합니다:

* __완전 선점 커널 모드(Fully Preemptible Kernel, CONFIG_PREEMPT_RT):__ 이 모드는 커널 내의 거의 모든 부분이 선점될 수 있도록 합니다. 이는 우선순위가 높은 작업이 최소한의 지연으로 실행될 수 있도록 보장합니다.
* __스핀락(spinlocks)을 뮤텍스(mutexes)로 대체:__ 스핀락은 대기 중인 스레드가 CPU 시간을 소모하며 락을 획득할 때까지 루프를 계속 실행하는 방식입니다. PREEMPT-RT는 이러한 스핀락을 스레드가 대기 상태에 들어가면서 CPU 시간을 절약할 수 있는 뮤텍스로 대체합니다.
* __고정 시간 인터럽트 핸들러와 스레드형 인터럽트 핸들러:__ 인터럽트 처리를 개선하여 시스템의 예측 가능성을 높입니다. 이는 인터럽트가 발생했을 때, 처리 시간을 최소화하고 인터럽트 처리를 보다 예측 가능하게 만듭니다.
* __우선순위 상속(priority inheritance):__ 우선순위 역전 문제를 해결하기 위해 도입된 메커니즘입니다. 더 낮은 우선순위의 스레드가 더 높은 우선순위의 스레드보다 먼저 실행되어야 할 때, 낮은 우선순위의 스레드가 높은 우선순위를 일시적으로 상속받아 문제를 해결합니다.

PREEMPT-RT 패치를 적용한 커널은 실시간 운영 체제(RTOS)의 특성을 갖게 되어, 실시간 작업을 처리하는 데 필요한 예측 가능한 타이밍과 성능을 제공합니다. 이 패치는 리눅스 커널의 실시간 성능을 크게 향상시켜, 실시간 요구 사항이 있는 다양한 산업 분야에서 리눅스의 사용을 가능하게 합니다.
### Avoid Bottom-half Lock Patch
`PREEMPT-RT` 패치를 통해 리눅스 커널의 실시간성을 높였지만, 아직 해결하지 못한 부분이 존재합니다.

예를들어 실시간 응용이 네트워크로 부터 패킷을 기다리는 상황에서는 아래와 같은 경로로 패킷이 들어오게 됩니다.

<img width="586" alt="image" src="https://github.com/CompactEdge/CompactEdge-OS/assets/60088469/2668b25d-b804-4189-9456-93f313e80cd8">

위 경로에서 kernel device driver에 도착한 패킷이 overlay network에 도착하기 전, `softirq`라고 불리는 후 처리 인터럽트 핸들러가 호출됩니다.

이 `softirq`는 패킷처리 외에 다른 여러 작업을 병행하여 실시간 성능에 큰 영향을 주게됩니다.

문제는 실시간 응용이 `softirq`가 수행중인 cpu에 스케줄 되는 경우, `softirq`의 동작을 방해하여 결과적으로 큰 delay가 발생할 수 있습니다.

실제로 분석을 진행한 결과 다음의 __worst case__가 발생하는 것을 확인하였습니다.

<img width="602" alt="image" src="https://github.com/CompactEdge/CompactEdge-OS/assets/60088469/c6a80a23-102e-408f-b959-56a8a089f919">

이 문제를 해결하기 위해 실시간 응용이 `softirq`를 회피하는 알고리즘을 적용하여 `softirq`의 동작을 보장하고, 실시간 응용이 빠르게 패킷을 받을 수 있도록 개선하였습니다.

<img width="698" alt="image" src="https://github.com/CompactEdge/CompactEdge-OS/assets/60088469/8e867968-58b6-4bc1-8a77-acaadee1c427">

<img width="453" alt="image" src="https://github.com/CompactEdge/CompactEdge-OS/assets/60088469/b85a10fc-f1b2-4868-a7d5-5d48633c21ba">

자체 실험을 진행한 결과, 패킷이 머신에 들어와서 응용에 도달하기 까지의 시간이 기존 `PREEMPT-RT`커널 대비 최악의 경우에서 약 30배 빨라진 것을 확인하였습니다.

<img width="611" alt="image" src="https://github.com/CompactEdge/CompactEdge-OS/assets/60088469/0229f240-f24b-407b-b906-16687948264a">

## Kubernetes
### Realtime Container
실시간 응용을 실행하기 위해서는 응용에 실시간 우선순위(realtime priority)를 부여해야 합니다.

기존의 kubernetes에서는 컨테이너 응용에 실시간 우선순위를 부여하는 기능이 제공되지 않습니다.

CompactEdge는 pod 생성 yaml파일의 `label`을 통해 실시간 우선순위를 부여할 수 있는 권한을 획득할 수 있습니다.

### RT Core
실시간 응용의 성능을 보장하기 위해서는 다른 응용이 접근하지 못하는 격리된 cpu가 필요합니다.

CompactEdge는 실시간 응용이 할당받을 수 있는 `RT-Core`를 정의하여 다른 응용이 접근하지 못하는 cpu 할당을 보장합니다.

<img width="559" alt="image" src="https://github.com/CompactEdge/CompactEdge-OS/assets/60088469/be123d58-37f4-485f-856f-2f0600bd1072">

격리된 실시간 컨테이너 응용은 softirq 회피 패치가 적용되어 다음과 같이 다른 응용에 방해받지 않습니다.

<img width="537" alt="image" src="https://github.com/CompactEdge/CompactEdge-OS/assets/60088469/7b1f0e2e-bb0a-457b-b433-0379ab9ec1b4">

### RT Core with kube-scheduler
`RT-Core`는 kubernetes 노드별로 어떤 cpu를 실시간 코어로 설정할지 지정할 수 있으며, 동적으로 변경할 수 있습니다.

실시간 응용을 배포할 경우 kube-scheduler에서 응용이 요구하는 cpu 사용량을 파악하여 할당 가능한 kubernetes 노드를 찾습니다.

<img width="1043" alt="image" src="https://github.com/CompactEdge/CompactEdge-OS/assets/60088469/c30253d3-ba6b-43e6-9627-38875fba6ec8">

남은 `RT-Core` 정보는 kubernetes 노드의 label을 통해 보여지며 이를 통해 실시간 응용이 실시간 코어를 할당받음을 보장할 수 있습니다.

# Architecture


# RT Linux patched version

Base Kernel version is 5.4.93 with preempt-rt patch.

The `linux-stable-5.4.93-rt51` branch has put base kernel sources.

The `linux-stable-5.4.93-rt51-avoid_bh_lock` branch has applied with softirq avoidance patch.

The `linux-stable-5.4.93-rt51-remove_bh_lock` branch is experimental branch. Be careful to use.
