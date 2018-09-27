node('linux && docker') {
    def dockerImageName = "gfieni/hwpc-sensor:${env.BUILD_TAG}"

    stage('git checkout') {
        checkout scm
    }

    stage('docker build') {
        sh "docker build -t ${dockerImageName} --build-arg BUILD_TYPE=release --target builder ."
    }
}
