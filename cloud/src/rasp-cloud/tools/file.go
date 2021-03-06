//Copyright 2017-2019 Baidu Inc.
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//http: //www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//limitations under the License.

package tools

import (
	"os"
	"strings"
	"io/ioutil"
	"sort"
	"path/filepath"
	"os/exec"
	"errors"
)

func GetCurrentPath() (string, error) {
	file, err := exec.LookPath(os.Args[0])
	if err != nil {
		return "", err
	}
	path, err := filepath.Abs(file)
	if err != nil {
		return "", err
	}
	i := strings.LastIndex(path, "/")
	if i < 0 {
		i = strings.LastIndex(path, "\\")
	}
	if i < 0 {
		return "", errors.New(`error: Can't find "/" or "\"`)
	}
	return string(path[0 : i+1]), nil
}

func ReadFromFile(path string) ([]byte, error) {
	data, err := ioutil.ReadFile(path)
	if err != nil {
		return data, err
	}

	return data, nil
}

func ListFiles(dirPath string, suffix string, prefix string) (files []string, err error) {
	dir, err := ioutil.ReadDir(dirPath)
	if err != nil {
		if os.IsPermission(err) {
			err = os.Chmod("plugin", os.ModePerm)
			if err == nil {
				dir, err = ioutil.ReadDir(dirPath)
			}
		}
		if err != nil {
			return
		}
	}

	for _, file := range dir {
		if !file.IsDir() {
			if strings.HasSuffix(file.Name(), suffix) && strings.HasPrefix(file.Name(), prefix) {
				files = append(files, file.Name())
			}
		}
	}

	sort.Sort(sort.Reverse(sort.StringSlice(files)))
	return files, nil
}

func PathExists(path string) (bool, error) {
	_, err := os.Stat(path)
	if err == nil {
		return true, nil
	}
	if os.IsNotExist(err) {
		return false, nil
	}
	return false, err
}
